// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include <string>
#include <vector>
#include <array>
#include <initializer_list>

namespace pugi
{
	class xml_document;
	class xml_node;
};




/**
 * Helper class for communicating via an XmlFSocket using MbML (Matlab Markup Language, see Almeida et al., 2003).
 * 
 * The class contains only static helper methods and is currently hard-wired to use pugixml.
 * 
 * Elements of all types can be added with the AddChild() method. Struct elements are used to construct a hierarchical document, which converts on the Matlab
 * side to a struct hierarchy. Data elements are created either by first creating the actual data element with AddChild() and then filling in data incrementally
 * with the data adder methods (...), or by providing a content string directly to AddChild().
 * 
 * The data items for multidimensional elements are added linearly, according to Matlab's column-major order (the first dimension is contiguous).
 * Multidimensional structs can be added simply by adding all fields of the first struct, then all fields of the second struct, and so on.
 * 
 * Almeida, J.S., Wu, S., and Voit, E.O. (2003). XML4MAT: Inter-conversion between MatlabTM structured variables and the markup language MbML.
 * Computer Science Preprint Archive (Elsevier), 2003(12):9-17.
 * http://hdl.handle.net/1853/12278, http://www.mathworks.com/matlabcentral/fileexchange/6268-xml4mat-v2-0
 */
class Mbml
{
public:

	/** Floating point precision specifiers. */
	enum Precision { SinglePrecision, DoublePrecision };


	// MbML element adders

	/** Add an MbML struct element to the document.
	 ** 
	 ** Struct fields can be added by calling the element adder methods for the returned element handle and providing the field name in the 'name' argument.
	 ** Dimensionality defaults to a scalar struct, but struct arrays are supported; simply add all fields of the first struct,
	 ** then all fields of the second struct, and so on.  */
	static pugi::xml_node AddStructArray( pugi::xml_node parent, const std::string & name, const std::vector<int> & dimensions = {1, 1} );

	/** Add an MbML cell element to the document.
	 ** 
	 ** Cell contents can be added by calling the element adder methods for the returned element handle and specifying "cell" as the element name. */
	static pugi::xml_node AddCellArray( pugi::xml_node parent, const std::string & name, const std::vector<int> & dimensions );

	/** Add an MbML char array (string) element to the document.
	 ** 
	 ** The dimensionality is inferred from the content. */
	static pugi::xml_node AddCharArray( pugi::xml_node parent, const std::string & name, const std::string & content );

	/** Add an MbML matrix element to the document.
	 ** 
	 ** The 'type' argument should correspond to a valid Matlab type specifier ("double", "single", "int16", ...).
	 ** The content is not processed in any way and is expected to correspond a textual representation of the data.
	 ** If no dimensionality is provided, then scalar dimensionality ("1 1") is assumed; dimensionality is never inferred from the provided content!
	 ** 
	 ** On success, a pugi reference to the appended child node is returned.
	 ** On failure, the pugi null reference is returned and nothing is added to the document. */
	static pugi::xml_node AddMatrix( pugi::xml_node parent, const std::string & name, const std::string & type, const std::string & content,
		const std::vector<int> & dimensions = {1, 1} );
	

	// element data (PCDATA) adders

	//static bool Set ... add float or double data .. maybe check the precision from the node attribute, if not too expensive/complex? just add 1-dimensional data and let 

};
