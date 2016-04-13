// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include <SharedPointer.h>

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <limits>

#include <Networking.h>




/**
 * Xml wrapper for FSockets that supports both xml-based and line-based communications. Incoming lines can be terminated with LF or CRLF; outgoing lines are
 * terminated with LF.
 * 
 * Xml documents received from the socket must be preceded by a block header and followed by a block footer as follows:
 *   XML_DOCUMENT_BEGIN
 *   <the xml document>
 *   XML_DOCUMENT_END
 * All outgoing xml documents are preceded by similar block headers and footers.
 * 
 * Network error and EOF handling has some limitations due to a UE issue (see the documentation of IsGood() for details). In practice, EOF (remote shutdown) and
 * network errors can be detected only in blocking mode. Also, data cannot be sent back to the remote once an EOF is encountered; the connection will be closed
 * immediately after arriving at EOF.
 * 
 * The socket can be closed by calling Close() or by destructing the XmlFSocket object. Both will perform a graceful shutdown.
 * 
 * If you are going to communicate with Matlab, then you might want to take a look at the Mbml helper class.
 *
 * Warning: No flood protection! The line buffer size is unlimited.
 * 
 * @see Mbml
 */
class XmlFSocket
{


	/* socket and line layer */


public:

	/** Constructs a new XmlFSocket wrapper around the provided FSocket. The socket argument can be null, in which case the resulting object will be in an
	 *  invalid state (IsGood() == false). */
	XmlFSocket( std::unique_ptr<FSocket> socket );


	/** Performs a graceful close of the socket. Destructing an XmlFSocket object directly will also perform a graceful close. Return true on success. */
	bool Close();

	/** Check whether we have a socket, that it is connected, that we have not reached EOF (remote shutdown), and no network errors have occurred.
	 *  
	 *  Note: Due to a UE limitation, this method might return a false positive in some cases. More specifically, if the socket is set to non-blocking (via a
	 *  SetBlocking(false) or SetBlocking(true, 0) call), then a received EOF (remote shutdown) and possibly also network errors cannot be detected at all and
	 *  IsGood() will return a false positive. If the socket is set to block (via a SetBlocking(true, n) call with n > 0), then an EOF and network errors can be
	 *  detected heuristically and IsGood() will work as expected. For the UE issue, see https://answers.unrealengine.com/questions/137371/ */
	bool IsGood() const;

	/** Set whether the read methods should block until success. Timeout is specified in milliseconds. Note that a timeout value of 0 does _not_ mean
	 *  "no timeout" but "don't block"! Write methods will never retry upon failure. */
	void SetBlocking( bool shouldBlock, int timeoutMs = std::numeric_limits<int>::max() );


	/** Prepares Buffer for further processing by removing all leading "garbage". More specifically, checks for the presence of an in-situ xml parse and, if one
	 *  is present, drops it and resets InXml. Then drops all leading whitespace from the buffer (whitespace as in std::isspace, in practice: spaces, tabs, LFs
	 *  and CRs). */
	void CleanupBuffer();

	/** Tries to read more data from the socket into Buffer. Note that GetLine() calls will leave the line terminator (LF or CRLF) of the read line into Buffer.
	 *  In addition to such line terminators, the beginning of the buffer might contain also an in-situ parse of an xml document, in case that GetXml() has been
	 *  just called. You can precede your calls to GetRaw() with a CleanupBuffer() call: this will drop a possibly existing in-situ xml parse and any leading
	 *  whitespace from Buffer.
	 *  
	 *  If ShouldBlock == true, then the call blocks until either more data or EOF has been received, BlockingTimeoutMs is exceeded, or a network error occurs.
	 *  
	 *  @return True if any new data was read, false otherwise. */
	bool GetRaw();

	/** Writes raw data to the socket. Return true on success, false on full or partial failure. */
	bool PutRaw( const void * buffer, std::size_t length );

	/** Tries to read the next non-empty, complete (LF or CRLF terminated) line from Buffer, which in turn is re-filled from the socket if necessary. On
	 *  success, the new line is placed into Line and removed from Buffer. The Line field is not touched upon failure. Buffer might have been cleaned up with
	 *  CleanupBuffer() in both cases.
	 *  
	 *  If ShouldBlock == true, then the call blocks until either success, there is no incoming activity for BlockingTimeoutMs, EOF is received, or a network
	 *  error occurs.
	 *  
	 *  @return True if a new line was read, false otherwise. */
	bool GetLine();

	/** Writes the contents of 'line' to the socket after appending an LF to it.
	 *
	 *  @return True on success, false on full or partial failure. */
	bool PutLine( std::string line );


	/** The UE FSocket. */
	std::unique_ptr<FSocket> Socket;

	/** Inbound data buffer. Might contain an in-situ parse of an xml document. */
	std::string Buffer;

	/** A copy of the last full line read with GetLine(), without the terminating LF or CRLF. This buffer can be modified directly. */
	std::string Line;

	/** If true, then all communications will be dumped to the log. */
	bool LogAllCommunications = false;


private:

	/** Tries to extract a complete, non-empty line from Buffer. On success, the line is placed in Line and true is returned.
	 *   The Line field is not touched on failure. */
	bool ExtractLineFromBuffer();


	/** If this is non-zero, then the Buffer contains an in-situ parse of an xml document. Further read operations should first remove this much data
	 *  from the beginning of the buffer and make sure that the related xml document (InXml) is not used afterwards.
	 *  Note that the Buffer data to be removed can contain nulls! */
	std::size_t BufferInSituXmlLength = 0;

	/** Whether read operations should block. */
	bool ShouldBlock = false;

	/** Network read timeout value for blocking read operations, in milliseconds. Note that a single read operation might perform several network reads, and
	 *  this value controls the timeout of such single _network_ read operations. */
	int32 BlockingTimeoutMs = 0;




	/* xml layer */


public:

	/**
	* Tries to read the next complete xml document from the socket. A proper xml block header is expected (see class documentation for details).
	* Preceding garbage data is _not_ skipped, except for whitespace (spaces, tabs, LFs and CRs). 
	* Note that the current InXml document is reset no matter whether a new xml document was found for parsing!
	* 
	* If ShouldBlock == true, then the call blocks until either success, there is no incoming activity for BlockingTimeoutMs, EOF is received, or a network
	* error occurs.
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
	 * @xmlDoc A pointer to the xml document to be sent. If null, then the XmlFSocket::OutXml document will be sent.
	 * @return True on success, false on full or partial failure.
	 */
	bool PutXml( pugi::xml_document * xmlDoc = 0 );


	/** The last XML document received with GetXml(). After a successful GetXml() call, the document is an in-situ parse of the XmlFSocket's Buffer field, with
	 *  the implication that any subsequent GetLine() or GetXml() read operation on this XmlFSocket needs to reset this document. */
	pugi::xml_document InXml;

	/** Parse status of InXMl, set by GetXml(). If GetXml() has not been called yet or if InXMl has been reset due to a subsequent (attempted) read operation,
	 *  then InXmlStatus.status is set to pugi::status_no_document_element. This field can be also queried as a bool, in which case it evaluates to true
	 *  if and only if InXmlStatus.status == pugi::status_ok. */
	pugi::xml_parse_result InXmlStatus;

	/** A pre-allocated, re-usable xml document that can be sent with SendXml(). If one is sending repeatedly an xml document with the same structure
	 *  with only the contained data changing, then it can be handy to initialize this document once and then just update the contained data
	 *  before each send operation. OutXml is never written to or reset by XmlFSocket itself; it is up to client code to use it in whatever way seems best. */
	pugi::xml_document OutXml;


private:

	/** Return codes for ExtractXmlFromBuffer(). */
	enum class ExtractXmlStatus { Ok, NoXmlBlockFound, ParseError };

	/** Tries to extract a complete xml document from Buffer. On success, the parsed document will reside in InXml and true is returned. Parsing is done
	 *  in-situ, and the BufferInSituXmlLength variable is set to indicate the length of the xml-reserved block sitting at the beginning of Buffer.
	 * 
	 *  InXml is not touched if the method fails due to not being able to locate an xml block in the buffer (ExtractXmlStatus::NoXmlBlockFound). However, in
	 *  case of a parse error (ExtractXmlStatus::ParseError), InXml should be considered invalid; InXmlStatus will indicate its exact state in this case.
	 *  
	 *  Also, in the case of a parse error of a complete xml block (ExtractXmlStatus::ParseError), the BufferInSituXmlLength field will be set in such a way
	 *  that a following CleanupBuffer() call will drop the corrupt xml document from Buffer.
	 *  
	 *  @see ExtractXmlStatus */
	ExtractXmlStatus ExtractXmlFromBuffer();

};
