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

	/** Element types. */
	enum Type { Struct, Char, Double, Single, TYPE_MAX };   // Remember to keep TypeNames in sync with this!

	/** Floating point precision specifiers. */
	enum Precision { SinglePrecision, DoublePrecision };


	// MbML element adders

	/** Add a new MbML element with the name 'name' under the given node.
	 ** 
	 ** If 'type' is anything else than 'Struct', then a PCDATA sub-element is also added and initialized to 'content'.
	 ** Note that you cannot directly add a PCDATA sub-element to a Struct element!
	 ** 
	 ** If 'dimensions' is left empty and 'type' is 'Char', then the dimensionality is automatically inferred from the provided string.
	 ** 
	 ** On success, a pugi reference to the appended child node is returned. On failure, the pugi null reference is returned. */
	static pugi::xml_node AddChild( pugi::xml_node & node, const std::string & name, Type type = Struct,
		std::vector<int> dimensions = {}, const std::string & content = std::string() );
	
	/** Helper overload for adding Char content. */
	static pugi::xml_node AddChild( pugi::xml_node & node, const std::string & name, const std::string & content );


	// element data (PCDATA) adders

	//static bool Set ... add float or double data .. maybe check the precision from the node attribute, if not too expensive/complex? just add 1-dimensional data and let 


private:

	/** Matlab strings corresponding to the Type enumeration. (VS requires the double braces) */
	static const std::array<const char *, TYPE_MAX> TypeNames;

};
