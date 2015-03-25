// Fill out your copyright notice in the Description page of Project Settings.

#include "RagdollController.h"
#include "Mbml.h"

#include <pugixml.hpp>

#include <string>
#include <sstream>
#include <vector>
#include <array>




pugi::xml_node Mbml::AddStructArray( pugi::xml_node parent, const std::string & name, const std::vector<int> & dimensions /*= { 1, 1 }*/ )
{
	return AddMatrix( parent, name, "struct", "", dimensions );
}


pugi::xml_node Mbml::AddCellArray( pugi::xml_node parent, const std::string & name, const std::vector<int> & dimensions )
{
	return AddMatrix( parent, name, "cell", "", dimensions );
}


pugi::xml_node Mbml::AddCharArray( pugi::xml_node parent, const std::string & name, const std::string & content )
{
	return AddMatrix( parent, name, "char", content, { 1, (int)content.length() } );
}


pugi::xml_node Mbml::AddMatrix( pugi::xml_node parent, const std::string & name, const std::string & type, const std::string & content,
	const std::vector<int> & dimensions /*= {1, 1}*/ )
{
	bool ok = true;

	// create the child node
	pugi::xml_node child = parent.append_child( name.c_str() );
	ok &= !child.empty();

	// add the type attribute
	ok &= child.append_attribute( "class" ).set_value( type.c_str() );
	
	// add the size attribute
	std::stringstream sizeSStream;
	for( auto dimension : dimensions )
	{
		sizeSStream << dimension << ' ';
	}
	std::string sizeString( sizeSStream.str() );
	sizeString.pop_back();
	ok &= child.append_attribute( "size" ).set_value( sizeString.c_str() );

	// add contents if 'content' is not empty
	if( !content.empty() )
	{
		ok &= child.text().set( content.c_str() );
	}

	// check for errors and return
	if( !ok )
	{
		// failed, remove the child and return the null handle
		parent.remove_child( child );
		return pugi::xml_node();
	}
	else
	{
		// all ok, return the new child node
		return child;
	}
}
