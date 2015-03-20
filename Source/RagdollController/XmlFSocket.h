// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include <SharedPointer.h>

#include <pugixml.hpp>

#include <string>

class FSocket;




/**
* Non-blocking xml wrapper for FSockets that supports both xml-based and line-based communications.
* 
* Xml documents received from the socket must be preceded by a block header that states the byte length of the document. The header is a single LF or CRLF
* terminated line with the following syntax:
* XML_DOCUMENT_BEGIN <length>
* where <length> is the combined byte length of the xml document AND the block header (that is, counting starts from the X character of the word "XML" in the
* header). <length> can be padded with zeros or spaces, so as to make it fixed-width. For example, the block header
* XML_DOCUMENT_BEGIN        230
* is a valid header line, assuming that it is terminated with a single LF, making it 30 bytes long, and it is followed by a 200-byte xml document.
* 
* All outgoing xml documents are preceded by a similar block header.
* 
* Warning: No flood protection! The line buffer size is unlimited.
*/
class XmlFSocket
{

protected:

	/** Temporary buffer. Might contain an in-situ parse of an xml document; see BufferInSituXmlLength! */
	std::string Buffer;

	/** If this is non-zero, then the Buffer contains an in-situ parse of an xml document. Further read operations should first remove this much data
	 ** from the beginning of the buffer and make sure that the related xml document (InXml) is not used afterwards.
	 ** Note that the Buffer data to be removed can contain nulls! */
	std::size_t BufferInSituXmlLength = 0;

	/** Whether read operations should block. */
	bool ShouldBlock = false;

	/** Network read timeout value for blocking read operations, in milliseconds. Note that a single read operation might perform several network reads, and
	 ** this value controls the timeout of such single _network_ read operations. */
	int32 BlockingTimeoutMs;


	/** Tries to read some more data from the socket into Buffer. Returns true if any new data was read. If ShouldBlock == true, then BlockingTimeoutMs
	 ** is adhered. */
	bool GetFromSocketToBuffer();

	/** Prepares Buffer for further processing. Drops leading whitespace (whitespace as in std::isspace, in practice: spaces, tabs, LFs and CRs).
	 ** Checks for the presence of an in-situ xml parse and, if one is present, drops it and resets InXml. */
	void CleanupBuffer();

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


public:

	/** The UE FSocket. */
	TSharedPtr<FSocket> Socket;

	/** A copy of the last full line read with GetLine(), without the terminating LF or CRLF. This buffer can be modified directly. */
	std::string Line;


	/** The last XML document received with GetXml(). The document is reset on the next read operation (GetLine or GetXml); the document is an in-situ
	 ** parse of the XmlFSocket's internal buffer, with the implication that any subsequent read operation on this XmlFSocket needs to reset it. */
	pugi::xml_document InXml;

	/** Parse status of InXMl, set by GetXml(). The status pugi::status_no_document_element means that either GetXml() has not been called yet, or InXMl has
	 ** been reset due to a subsequent (attempted) read operation. */
	pugi::xml_parse_result InXmlStatus;

	/** A pre-allocated, re-usable xml document that can be sent with SendXml(). If one is sending repeatedly an xml document with the same structure
	 ** with only the contained data changing, then it can be handy to initialize this document once and then just update the contained data
	 ** before each send operation. */
	pugi::xml_document OutXml;


	/**
	* Constructs a new XmlFSocket wrapper around the provided FSocket and shares its ownership via the provided shared pointer.
	* The socket argument can be null, in which case the resulting object will be in an invalid state (IsGood() == false).
	*/
	XmlFSocket( const TSharedPtr<FSocket> & socket );


	/** Check whether we have a socket and that it is connected and all-ok. */
	bool IsGood();

	/** Set whether the read methods should block until success. Timeout is specified in milliseconds (0 to disable).
	 ** Write methods will never retry upon failure. */
	void SetBlocking( bool shouldBlock, int timeoutMs = 0 );


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
	* Tries to read the next complete xml document from the socket. A proper xml block header is expected (see class documentation for details).
	* Preceding garbage data is _not_ skipped, except for possible preceding whitespace (spaces, tabs, LFs and CRs). 
	* Note that the current InXml document will be reset no matter whether a new xml document is available for parsing!
	* 
	* On success, the parsed document becomes available in InXml and InXmlStatus is set to pugi::status_ok. See the documentation of InXml for details.
	* On failure, InXmlStatus can be used to determine the state of InXml.
	* 
	* @return True if a new xml document was read successfully, false otherwise. See InXmlStatus for more information about the result.
	*/
	bool GetXml();

	/**
	 * Sends an xml document to the socket.
	 * 
	 * @xmlDoc A pointer to the RapidXML document to be sent. If null, then the XmlFSocket::OutXml document will be sent.
	 * @return True on success, false on full or partial failure.
	 */
	//bool PutXml( rapidxml::xml_document<> * xmlDoc = 0 );
};
