// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include <SharedPointer.h>

#define RAPIDXML_NO_EXCEPTIONS
#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

#include <string>

class FSocket;




/**
* Non-blocking xml wrapper for FSockets. Supports both xml-based and line-based communications.
*
* Warning: No flood protection! The line buffer size is unlimited.
* 
* Warning: We use RapidXML, and since we do not have exceptions in UE, we have only bad options when a parse error occurs. In this case, RapidXML is designed
* to produce undefined behavior unles one abort()'s after a parse error. However, we take that risk after logging the fact: on a parse error, we log it to
* our system log (LogRcSystem) as an error, and continue parsing as if nothing had happened.
*/
class XmlFSocket
{
	/** Is set to true in RapidXML's error handler. Will be reverted back to false */
	bool RapidXmlParseError;

	/** Helper counter used by ExtractXmlFromBuffer(). */
	std::size_t BufferXmlScanPos = 0;

	/** Helper counter used by ExtractXmlFromBuffer(). */
	int32 BufferXmlScanDepth = 0;


protected:

	/** Temporary buffer. Might contain an in-situ parse of an xml document; see BufferInSituXmlLength! */
	std::string Buffer;

	/** If this is non-zero, then the Buffer contains an in-situ parse of an xml document. Further read operations should first  */
	std::size_t BufferInSituXmlLength = 0;

	/** Whether read operations should block. */
	bool ShouldBlock = false;

	/** Timeout value for blocking read operations, in milliseconds. */
	int32 TimeoutMs;


	/**
	* Tries to extract a complete, non-empty line from Buffer. On success, the line is placed in Line and true is returned.
	* The Line field is not touched on failure.
	*/
	bool ExtractLineFromBuffer();

	/**
	* Tries to extract a complete xml document from Buffer. On success, the document is parsed to xmlDoc and true is returned.
	* Parsing is done in-situ, and the BufferInSituXmlLength variable is set to indicate the length of the xml-reserved block sitting at the beginning of
	* Buffer.
	* 
	* xmlDoc is not touched on failure, except for parse errors; see GetXml().
	*/
	bool ExtractXmlFromBuffer();

	/** Tries to read some more data from the socket into Buffer. Returns true if any new data was read. */
	bool GetFromSocketToBuffer();


public:

	/** The UE FSocket. */
	TSharedPtr<FSocket> Socket;

	/** A copy of the last full line read with GetLine(), without the terminating LF or CRLF. It is allowed to directly modify this buffer. */
	std::string Line;

	/** The last XML document received with GetXml(). The document is valid only until the next read operation (GetLine or GetXml); the document is an in-situ
	 ** parse of the XmlFSocket's internal buffer (we use RapidXML in destructive mode), with the implication that any read operation on this XmlFSocket will
	 ** invalidate the data pointers contained in this xml document. */
	rapidxml::xml_document<> InXml;

	/** A pre-allocated, re-usable xml document that can be sent with SendXml(). If one is sending repeatedly an xml document with the same structure
	 ** (with only the contained data changing), then it can be handy to initialize this document once and then just update the contained data
	 ** before each send operation. */
	rapidxml::xml_document<> OutXml;


	/**
	* Constructs a new XmlFSocket wrapper around the provided FSocket and shares its ownership via the provided shared pointer.
	* The socket argument can be null, in which case the resulting object will be in an invalid state (IsGood() == false).
	*/
	XmlFSocket( const TSharedPtr<FSocket> & socket );


	/** Check whether we have a socket and that it is connected and all-ok. */
	bool IsGood();

	/** Set whether the read methods should block until success. Timeout is specified in milliseconds. Write methods will never retry upon failure. */
	void SetBlocking( bool shouldBlock, int timeoutMs );


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

	/**
	* Tries to read the next complete xml document from the socket. On success, the new document is parsed into InXml.
	* See the documentation of InXml for details. InXml is not touched if there is no full xml document available in the socket buffer.
	* However, a parse error of a full document will leave the document _and the whole system_ in an undefined state! See the class documentation for details.
	*
	* @return True if a new xml document was read, false otherwise.
	*/
	bool GetXml();

	/**
	 * Sends an xml document to the socket.
	 * 
	 * @xmlDoc A pointer to the RapidXML document to be sent. If null, then the XmlFSocket::OutXml document will be sent.
	 * @return True on success, false on full or partial failure.
	 */
	bool PutXml( rapidxml::xml_document<> * xmlDoc = 0 );
};
