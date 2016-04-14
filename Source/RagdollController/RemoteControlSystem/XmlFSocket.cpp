// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "XmlFSocket.h"

#include <Sockets.h>

#include <pugixml.hpp>

#include <boost/interprocess/streams/bufferstream.hpp>

#include <memory>
#include <algorithm>
#include <functional>
#include <cctype>


/** Preallocation size for various internal buffers. */
#define PREALLOC_SIZE (64 * 1024)


/** Threshold time (in seconds) for considering whether FSocket::Wait() has returned immediately. For a closed socket, Wait() returns in 1.5 to 50 microseconds
 *  (mean = 2.5, n >= 10000). A too low threshold will result in false negatives in EOF and network error detection. On the other hand, the threshold must be
 *  lower that the blocking timeout being used. Otherwise we would get false positives in EOF and network error detection. The smallest possible blocking
 *  timeout is 1 ms, so setting the threshold to 500 us seems to have a good margin in both directions. */
#define FSOCKET_WAIT_IMMEDIATE_RETURN_THRESHOLD (500.f * 1e-6)


/** Xml block header and footer tag */
#define XML_BLOCK_HEADER "XML_DOCUMENT_BEGIN"
#define XML_BLOCK_FOOTER "XML_DOCUMENT_END"


/** Log category for full debug dump */
#define RAW_DUMP_BLOCK_SIZE 500   // UE_LOG has an upper limit on the length of the string, so we need to chop up the string into small enough blocks
DECLARE_LOG_CATEGORY_EXTERN( LogXmlFSocketDumpInbound, Log, All );
DECLARE_LOG_CATEGORY_EXTERN( LogXmlFSocketDumpOutbound, Log, All );
DEFINE_LOG_CATEGORY( LogXmlFSocketDumpInbound );
DEFINE_LOG_CATEGORY( LogXmlFSocketDumpOutbound );




XmlFSocket::XmlFSocket( std::unique_ptr<FSocket> socket ) :
	Socket( std::move( socket ) )
{
	InXmlStatus.status = pugi::status_no_document_element;
}




bool XmlFSocket::Close()
{
	return Socket && Socket->Close();
}




bool XmlFSocket::IsGood() const
{
	return this->Socket && this->Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}




void XmlFSocket::SetBlocking( bool shouldBlock, int blockingTimeoutMs /*= max int*/ )
{
	ShouldBlock = shouldBlock;
	BlockingTimeoutMs = blockingTimeoutMs;
}




bool XmlFSocket::GetLine()
{
	// read more data until either we have a full line or no more new data
	do
	{
		// see if Buffer has a complete line, in which case extract it and return
		if( ExtractLineFromBuffer() ) return true;

	} while( GetRaw() );

	// no more data available and did not get a complete line
	return false;
}




bool XmlFSocket::PutLine( std::string line )
{
	// append LF (prefer a possible string copy in place of two Send() calls and risking network fragmentation)
	// (could avoid the copy/copies by temporarily abusing the null terminator..)
	line.append( "\n" );

	// write data and return
	return PutRaw( (const void *)line.c_str(), line.size() );
}




bool XmlFSocket::GetXml()
{
	// read more data until either we have a full document, an error state, or no more new data
	do
	{
		switch( ExtractXmlFromBuffer() )
		{
		case ExtractXmlStatus::Ok:
			// all ok -> stop and return success
			return true;
			break;
		case ExtractXmlStatus::NoXmlBlockFound:
			// no xml block found yet -> keep reading more data in
			break;
		case ExtractXmlStatus::ParseError:
			// xml block found but it is invalid -> return error
			return false;
			break;
		default:
			check( false );   // should not be reached
		}
	} while( GetRaw() );

	// no more data available and did not get a complete document
	return false;
}




bool XmlFSocket::PutXml( pugi::xml_document * xmlDoc /*= 0 */ )
{
	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

	// Use OutXml if no document given
	if( !xmlDoc )
	{
		xmlDoc = &this->OutXml;
	}

	// create the writer object for pugi
	class Writer : public pugi::xml_writer
	{
		XmlFSocket & Socket;

	public:
		bool IsGood;

		Writer( XmlFSocket & socket ) : Socket( socket ), IsGood( Socket.IsGood() ) {}

		virtual void write( const void* data, std::size_t size )
		{
			if( !IsGood || !Socket.Socket ) return;
			IsGood &= Socket.PutRaw( data, size );
		}
	} writer( *this );

	// write to socket
	bool headerFooterOk = true;
	headerFooterOk &= this->PutLine( XML_BLOCK_HEADER );
	xmlDoc->save( writer );
	headerFooterOk &= this->PutLine( XML_BLOCK_FOOTER );

	// return the success status
	return headerFooterOk && writer.IsGood;
}




void XmlFSocket::CleanupBuffer()
{
	// make sure that we do not have an in-situ xml parse in Buffer
	Buffer.erase( 0, BufferInSituXmlLength );
	BufferInSituXmlLength = 0;

	// reset InXml and its status
	InXml.reset();
	InXmlStatus = pugi::xml_parse_result();
	InXmlStatus.status = pugi::status_no_document_element;

	// drop leading whitespace
	// copied from https://stackoverflow.com/questions/216823/whats-the-best-way-to-trim-stdstring
	Buffer.erase( Buffer.begin(), std::find_if( Buffer.begin(), Buffer.end(), std::not1( std::ptr_fun<int, int>( std::isspace ) ) ) );
}




bool XmlFSocket::ExtractLineFromBuffer()
{
	// skip leading whitespace, drop previous in-situ InXml
	CleanupBuffer();

	// now, do we have a complete line at the beginning of Buffer? look for a CR or LF
	std::size_t contentLength = this->Buffer.find_first_of( "\r\n" );
	if( contentLength != std::string::npos )
	{
		// we have a line, extract it and return true
		this->Line.assign( this->Buffer, 0, contentLength );
		this->Buffer.erase( 0, contentLength );
		return true;
	}
	else
	{
		// no full line, return false
		return false;
	}
}




XmlFSocket::ExtractXmlStatus XmlFSocket::ExtractXmlFromBuffer()
{
	// skip leading whitespace, drop previous in-situ InXml
	CleanupBuffer();

	// check if we have an xml block header at the beginning of the buffer. return false if not.
	if( 0 != Buffer.compare( 0, std::strlen( XML_BLOCK_HEADER ), XML_BLOCK_HEADER ) ) return ExtractXmlStatus::NoXmlBlockFound;

	// check if we have an xml block footer somewhere in the buffer (keep it simple and don't care about the line terminator). return false if not.
	std::size_t footerPos = Buffer.find( XML_BLOCK_FOOTER );
	if( footerPos == std::string::npos ) return ExtractXmlStatus::NoXmlBlockFound;

	// we have a complete xml block in the buffer, now try to parse it into InXml (let pugixml eat the block header).
	// Set BufferInSituXmlLength so that the footer is discarded too when the in-situ parse is cleared (let the line terminator stay).
	BufferInSituXmlLength = footerPos + std::strlen( XML_BLOCK_FOOTER );
	InXmlStatus = InXml.load_buffer_inplace( &Buffer[0], footerPos );

	// if xml parse errors, the return false
	if( !InXmlStatus ) return ExtractXmlStatus::ParseError;

	// all ok, return true
	return ExtractXmlStatus::Ok;
}




/** Due to a UE limitation, we cannot explicitly detect an EOF (shutdown) and probably not even network errors (see documentation of IsGood()). However, for a
 *  blocking socket, we can infer such cases from the time that FSocket::Wait() takes: Wait() returns immediately for EOFed or failed sockets even in blocking
 *  mode. In blocking mode (with a long enough blocking time), we can then infer that there is an EOF or a network error if Wait() returns immediately, but
 *  there is still no pending data in the socket. */
bool XmlFSocket::GetRaw()
{
	bool success;
	uint32 bytesPending;
	int32 bytesRead;

	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

	// if in blocking mode, wait until we have new data or EOF, or a network error occurs
	bool WaitReturnedImmediately = true;
	if( this->ShouldBlock )
	{
		// UE's Wait() is not well documented, but at least most platform implementations seem to use select() in a way that returns immediately on network
		// errors or EOF (as of UE 4.9). However, we need a heuristic to detect EOF and network errors; see method implementation documentation.
		double t0 = FPlatformTime::Seconds();
		this->Socket->Wait( ESocketWaitConditions::WaitForRead, FTimespan( 0, 0, 0, 0, this->BlockingTimeoutMs ) );
		float WaitTime = FPlatformTime::Seconds() - t0;

		WaitReturnedImmediately = WaitTime < FSOCKET_WAIT_IMMEDIATE_RETURN_THRESHOLD;
	}

	// check how much new data we have. also try to detect EOF and network errors.
	success = this->Socket->HasPendingData( bytesPending );
	if( this->ShouldBlock && this->BlockingTimeoutMs >= 1 && WaitReturnedImmediately && (!success || bytesPending == 0) )
	{
		// blocking Wait() returned immediately, but there is no pending data -> infer that the connection has EOFed or failed -> drop the connection and return
		this->Socket.reset();
		return false;
	}
	// return false if nothing new
	if( !success || bytesPending == 0 ) return false;

	// invariant: we have pending data

	// allocate space and read the data
	this->Buffer.resize( this->Buffer.size() + bytesPending );
	success = this->Socket->Recv( (uint8 *)&this->Buffer[this->Buffer.size() - bytesPending], bytesPending, bytesRead, ESocketReceiveFlags::None );

	if( !success )
	{
		// socket failure: revert the size of Buffer, then return false
		this->Buffer.resize( this->Buffer.size() - bytesPending );
		return false;
	}

	// success: correct the size of Buffer in case that bytesRead < bytesPending
	this->Buffer.resize( this->Buffer.size() - bytesPending + bytesRead );

	// dump to log?
	if( LogAllCommunications )
	{
		std::string s( Buffer.substr( Buffer.length() - bytesRead, bytesRead ) );
		for( std::size_t pos = 0; pos < s.length(); pos += RAW_DUMP_BLOCK_SIZE )
		{
			UE_LOG( LogXmlFSocketDumpInbound, Log, TEXT( "\n%s" ), *FString( s.substr( pos, RAW_DUMP_BLOCK_SIZE ).c_str() ) );
		}
	}

	return true;
}




bool XmlFSocket::PutRaw( const void * buffer, std::size_t length )
{
	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

	// write data
	int32 bytesSent;
	this->Socket->Send( (const uint8 *)buffer, length, bytesSent );

	// dump to log?
	if( LogAllCommunications )
	{
		std::string s( (const char *)buffer, length );
		for( std::size_t pos = 0; pos < s.length(); pos += RAW_DUMP_BLOCK_SIZE )
		{
			UE_LOG( LogXmlFSocketDumpOutbound, Log, TEXT( "\n%s" ), *FString( s.substr( pos, RAW_DUMP_BLOCK_SIZE ).c_str() ) );
		}
	}

	if( bytesSent == length )
	{
		// success
		return true;
	}
	else
	{
		// failure: drop the connection and return false
		this->Socket.reset();
		return false;
	}
}
