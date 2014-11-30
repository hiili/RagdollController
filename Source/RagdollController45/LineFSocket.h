// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include <SharedPointer.h>

#include <string>

class FSocket;




/**
* Line-based non-blocking wrapper for FSockets.
*
* WARNING: No flood protection! The line buffer size is unlimited.
*/
class LineFSocket
{
protected:

	/** Temporary buffer. */
	std::string Buffer;


	/**
	* Tries to extract a complete, non-empty line from Buffer. On success, the line is placed in Line and true is returned.
	* The Line field is not touched on failure.
	*/
	bool ExtractFromBuffer();

	/** Tries to read some more data from the socket into Buffer. Returns true if any new data was read. */
	bool GetFromSocketToBuffer();


public:

	/** The UE FSocket. */
	TSharedPtr<FSocket> Socket;

	/** The contents of the last successfully read line, without the terminating LF or CRLF. It is allowed to directly modify this buffer. */
	std::string Line;


	/**
	* Constructs a new LineFSocket wrapper around the provided FSocket and shares its ownership via the provided shared pointer.
	* The socket argument can be null, in which case the resulting object will be in an invalid state (IsGood() == false).
	*/
	LineFSocket( const TSharedPtr<FSocket> & socket );


	/** Check whether we have a socket and that it is connected and all-ok. */
	bool IsGood();

	/**
	* Tries to read the next non-empty, complete (LF or CRLF terminated) line from the socket. On success, the new line is placed into Line.
	* The Line field is not touched upon failure.
	*
	* @return True if a new line was read, false otherwise.
	*/
	bool GetLine();

	/**
	 * Writes an LF-terminated line to the socket.
	 * 
	 * @return True on success, false on full or partial failure.
	 */
	bool PutLine( std::string line );
};
