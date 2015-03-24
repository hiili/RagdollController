// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "Mbml.h"

#include <pugixml.hpp>

#include <string>
#include <sstream>
#include <vector>
#include <array>

#include <algorithm>




const std::array<const char *, Mbml::TYPE_MAX> Mbml::TypeNames = { { "struct", "char", "double", "single" } };




pugi::xml_node Mbml::AddChild( pugi::xml_node & node, const std::string & name, Type type /*= Struct*/,
	std::vector<int> dimensions /*= {}*/, const std::string & content /*= std::string()*/ )
{
	bool ok = true;

	// create the child node
	pugi::xml_node child = node.append_child( name.c_str() );
	ok &= !child.empty();

	// add the type attribute
	if( !(type >= 0 && type < TYPE_MAX) ) return pugi::xml_node();
	ok &= child.append_attribute( "class" ).set_value( TypeNames[type] );
	
	// infer dimensionality if no dimensions given
	if( dimensions.size() == 0 )
	{
		if( type == Char )
		{
			// Char type, infer from 'content'
			dimensions = { 1, (int)content.length() };
		}
		else
		{
			// other type, default to scalar
			dimensions = { 1, 1 };
		}
	}

	// add the size attribute
	std::stringstream sizeString;
	for( auto dimension : dimensions )
	{
		sizeString << dimension << ' ';
	}
	ok &= child.append_attribute( "size" ).set_value( sizeString.str().c_str() );

	// add contents if 'content' is not empty. fail if type is Struct.
	if( !content.empty() )
	{
		if( type == Struct )
		{
			ok = false;
		}
		else
		{
			ok &= child.text().set( content.c_str() );
		}
	}

	// check for errors and return
	if( !ok )
	{
		// failed, remove the child and return the null handle
		node.remove_child( child );
		return pugi::xml_node();
	}
	else
	{
		// all ok, return the new child node
		return child;
	}
}


pugi::xml_node Mbml::AddChild( pugi::xml_node & node, const std::string & name, const std::string & content )
{
	return AddChild( node, name, Char, std::vector<int>(), content );
}
