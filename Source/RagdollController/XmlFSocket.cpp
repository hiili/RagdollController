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


/** Xml block header tag */
#define XML_BLOCK_HEADER "XML_DOCUMENT_BEGIN"




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




//bool XmlFSocket::PutXml( rapidxml::xml_document<> * xmlDoc /*= 0 */ )
//{
//	// check that we have a valid and connected socket
//	if( !IsGood() ) return false;
//
//	// Use OutXml if no document given
//	if( !xmlDoc )
//	{
//		xmlDoc = &this->OutXml;
//	}
//
//	// generate the xml string, stop and return false on failure
//	std::string str; str.reserve( PREALLOC_SIZE );
//	RapidXmlManager.ReserveRapidXml();
//	rapidxml::print( std::back_inserter( str ), *xmlDoc );
//	if( !RapidXmlManager.ReleaseRapidXml() ) return false;
//
//	// send the string to the socket
//	int32 bytesSent;
//	this->Socket->Send( (const uint8 *)str.data(), str.size(), bytesSent );
//
//	// return the success status
//	return bytesSent == str.size();
//}




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

	// wrap the Buffer in a non-copying bufferstream. limit its length to avoid unnecessary parsing if there is no xml block header but other data.
	boost::interprocess::bufferstream buffer( &Buffer[0], std::min<int>( 128, Buffer.length() ), std::ios_base::in );
	
	// try to parse the header string and the block length string
	std::string blockHeaderString;
	int blockLength;
	buffer >> blockHeaderString >> blockLength;

	// check if a header was successfully detected. return false if not.
	if( buffer.fail() || 0 != blockHeaderString.compare( XML_BLOCK_HEADER ) ) return false;

	// we have a header, now check if the entire document is already in the buffer. return false if not (keep it simple and do not cache already parsed header data).
	if( Buffer.length() < blockLength ) return false;

	// we have a valid xml document in the buffer, now try to parse it into InXml (let pugixml eat the block header). remember to set BufferInSituXmlLength!
	BufferInSituXmlLength = blockLength;
	InXmlStatus = InXml.load_buffer_inplace( &Buffer[0], blockLength );

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
