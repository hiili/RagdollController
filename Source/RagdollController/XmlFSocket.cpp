// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "XmlFSocket.h"

#include <Sockets.h>

#include <pugixml.hpp>

#include <boost/interprocess/streams/bufferstream.hpp>

#include <algorithm>
#include <functional>
#include <cctype>


/** Preallocation size for various internal buffers. */
#define PREALLOC_SIZE (64 * 1024)


/** Xml block header and footer tag */
#define XML_BLOCK_HEADER "XML_DOCUMENT_BEGIN"
#define XML_BLOCK_FOOTER "XML_DOCUMENT_END"




XmlFSocket::XmlFSocket( const TSharedPtr<FSocket> & socket ) :
	Socket( socket )
{
	InXmlStatus.status = pugi::status_no_document_element;
}




bool XmlFSocket::IsGood()
{
	return this->Socket.IsValid() && this->Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}




void XmlFSocket::SetBlocking( bool shouldBlock, int blockingTimeoutMs /*= 0*/ )
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

	} while( GetFromSocketToBuffer() );

	// no more data available and did not get a complete line
	return false;
}




bool XmlFSocket::PutLine( std::string line )
{
	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

	// append LF (prefer a possible string copy in place of two Send() calls and risking network fragmentation)
	line.append( "\n" );

	// write data
	int32 bytesSent;
	this->Socket->Send( (const uint8 *)line.data(), line.size(), bytesSent );

	// return the success status
	return bytesSent == line.size();
}




bool XmlFSocket::GetXml()
{
	// read more data until either we have a full document or no more new data
	do
	{
		// see if Buffer has a complete document, in which case extract it and return
		if( ExtractXmlFromBuffer() ) return true;

	} while( GetFromSocketToBuffer() );

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

		virtual void write( const void* data, size_t size )
		{
			if( !IsGood || !Socket.Socket.IsValid() ) return;

			int32 bytesSent;
			Socket.Socket->Send( (const uint8 *)data, size, bytesSent );
			IsGood &= bytesSent == size;
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
	// do we have an in-situ xml parse in Buffer?
	if( BufferInSituXmlLength > 0 )
	{
		// yes, drop it
		Buffer.erase( 0, BufferInSituXmlLength );
		BufferInSituXmlLength = 0;

		// reset InXml and its status
		InXml.reset();
		InXmlStatus = pugi::xml_parse_result();
		InXmlStatus.status = pugi::status_no_document_element;
	}

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




bool XmlFSocket::ExtractXmlFromBuffer()
{
	// skip leading whitespace, drop previous in-situ InXml
	CleanupBuffer();

	// check if we have an xml block header at the beginning of the buffer. return false if not.
	if( 0 != Buffer.compare( 0, std::strlen( XML_BLOCK_HEADER ), XML_BLOCK_HEADER ) ) return false;

	// check if we have an xml block footer somewhere in the buffer (keep it simple and don't care about the line terminator). return false if not.
	std::size_t footerPos = Buffer.find( XML_BLOCK_FOOTER );
	if( footerPos == std::string::npos ) return false;

	// we have a valid xml document in the buffer, now try to parse it into InXml (let pugixml eat the block header).
	// Set BufferInSituXmlLength so that the footer is discarded too when the in-situ parse is cleared (let the line terminator stay).
	BufferInSituXmlLength = footerPos + std::strlen( XML_BLOCK_FOOTER );
	InXmlStatus = InXml.load_buffer_inplace( &Buffer[0], footerPos );

	// check for parse errors
	if( !InXmlStatus ) return false;

	// all ok, return true
	return true;
}




bool XmlFSocket::GetFromSocketToBuffer()
{
	bool success;
	uint32 bytesPending;
	int32 bytesRead;

	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

	// if in blocking mode, wait until we have new data
	if( this->ShouldBlock )
	{
		this->Socket->Wait( ESocketWaitConditions::WaitForRead, FTimespan( 0, 0, 0, 0, this->BlockingTimeoutMs ) );
	}

	// check how much new data we have, return false if nothing new
	if( !this->Socket->HasPendingData( bytesPending ) ) return false;

	// allocate space and read the data
	this->Buffer.resize( this->Buffer.size() + bytesPending );
	success = this->Socket->Recv( (uint8 *)&this->Buffer[this->Buffer.size() - bytesPending], bytesPending, bytesRead, ESocketReceiveFlags::None );

	if( !success )
	{
		// socket failure: revert the size of Buffer, then return false
		this->Buffer.resize( this->Buffer.size() - bytesPending );
		return false;
	}

	// success: correct the size of Buffer in case that bytesRead < bytesPending, then return true
	this->Buffer.resize( this->Buffer.size() - bytesPending + bytesRead );
	return true;
}
