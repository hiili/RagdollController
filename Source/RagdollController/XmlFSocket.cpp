// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "XmlFSocket.h"

#include <Sockets.h>

#include <pugixml.hpp>


/** Preallocation size for various internal buffers. */
#define PREALLOC_SIZE (64 * 1024)




XmlFSocket::XmlFSocket( const TSharedPtr<FSocket> & socket ) :
	Socket( socket )
{}




bool XmlFSocket::IsGood()
{
	return this->Socket.IsValid() && this->Socket->GetConnectionState() == ESocketConnectionState::SCS_Connected;
}




void XmlFSocket::SetBlocking( bool shouldBlock, int blockingTimeoutMs )
{
	check( false );
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




bool XmlFSocket::ExtractXmlFromBuffer()
{
	check( false );
	return false;
}

bool XmlFSocket::ExtractLineFromBuffer()
{
	// seek over all leading CR and LF characters in Buffer
	std::size_t contentBegin = 0; std::size_t bufferSize = this->Buffer.size();
	while( contentBegin < bufferSize && (this->Buffer[contentBegin] == '\r' || this->Buffer[contentBegin] == '\n') ) ++contentBegin;

	// now, do we have a complete, non-empty line at the beginning of Buffer? look for a CR or LF
	std::size_t contentLength = this->Buffer.find_first_of( "\r\n", contentBegin );
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




bool XmlFSocket::GetFromSocketToBuffer()
{
	bool success;
	uint32 bytesPending;
	int32 bytesRead;

	// check that we have a valid and connected socket
	if( !IsGood() ) return false;

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
