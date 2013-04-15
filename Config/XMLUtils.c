/* 
 * 	Copyright (c) 2010 Colorado State University
 * 
 *	Permission is hereby granted, free of charge, to any person
 *	obtaining a copy of this software and associated documentation
 *	files (the "Software"), to deal in the Software without
 *	restriction, including without limitation the rights to use,
 *	copy, modify, merge, publish, distribute, sublicense, and/or
 *	sell copies of the Software, and to permit persons to whom
 *	the Software is furnished to do so, subject to the following
 *	conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *	OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 *  File: XMLUtils.c
 * Authors: He Yan
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "configdefaults.h"
#include "XMLUtils.h"
#include "configfile.h"
#include "../Util/log.h"
#include "../site_defaults.h"


/*----------------------------------------------------------------------------------------
 * Purpose: Trims spaces a tabs from the beginning and end of a string
 * Input:   the string to remove leading and trailing spaces from, in place
 * Output:  the trimmed string (modified in place)
 * Nick @ June 24, 2008   
 * -------------------------------------------------------------------------------------*/
void trim(char * str)
{
	if(str == NULL) return;
	
	//Trim trailing spaces
	int len = strlen(str);
	int i = 0;
	if(len > 0)
	{
		for(i = len - 1; i >= 0; i--)
		{
			if(str[i] == ' ' || str[i] == '\t' || str[i] == '\0') str[i] = '\0';
			else break;
		}
	}
	
	//Find number of leading spaces
	len = strlen(str);
	int shift = 0;
	while(shift < len && (str[shift] == ' ' || str[shift] == '\t'))
		shift++;

	//Trim leading spaces
	for(i = 0; i <= len - shift; i++)
		str[i] = str[i+shift];
}


 /*----------------------------------------------------------------------------------------
 * Purpose: return ta count of the nodes with the same xpath
 * Input:	
 * Output: the count
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
int 
getListCount( xmlDocPtr document, char *xpath )
{
	int number = 0;
	xmlXPathContextPtr context = xmlXPathNewContext( document );
	if ( context == NULL )
		return 0;
	
	xmlXPathObjectPtr object = xmlXPathEvalExpression( (xmlChar *)xpath, context );
	if ( object == NULL )
	{
		xmlXPathFreeContext( context );
		return 0;
	}
	xmlNodeSetPtr nodes = object->nodesetval;
	if ( nodes == NULL )
	{
		xmlXPathFreeContext( context );
		xmlXPathFreeNodeSetList( object );		
		return 0;	
	}
	
	number = nodes->nodeNr;
	xmlXPathFreeNodeSet( nodes );
	xmlXPathFreeNodeSetList( object );
	xmlXPathFreeContext( context );	
	return number;
}	

/*----------------------------------------------------------------------------------------
 * Purpose: return the value of a single node as a string
 * Input:	
 * Output: a trimed string
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
char *
getValueAsString( xmlDocPtr document, char *xpath, int maxLength )
{
	xmlXPathContextPtr context = xmlXPathNewContext( document );
	if ( context == NULL )
		return NULL;
	
	xmlXPathObjectPtr object = xmlXPathEvalExpression((xmlChar *)xpath, context );
	if ( object == NULL )
	{
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	xmlNodeSetPtr nodes = object->nodesetval;
	if ( nodes == NULL || nodes->nodeNr != 1 )
	{
		xmlXPathFreeContext( context );
		xmlXPathFreeNodeSetList( object );
		if (nodes != NULL)
			xmlXPathFreeNodeSet( nodes );
		return NULL;
	}
	
	xmlNodePtr element = nodes->nodeTab[0];
	if ( element->type != XML_ELEMENT_NODE )
	{
		xmlXPathFreeNodeSet( nodes );
		xmlXPathFreeNodeSetList( object );
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	char * content = NULL;
	// check to see if the element has a value
	// A tag with no characters between the tags will not count as a value
	if(element->children!=NULL) {
		xmlNodePtr text = element->children;
		if ( text->type != XML_TEXT_NODE )
		{
			xmlXPathFreeNodeSet( nodes );
			xmlXPathFreeNodeSetList( object );
			xmlXPathFreeContext( context );
			return NULL;
		}
		content = (char *)xmlNodeGetContent(text);
	}
	else {
		// return a blank string so code doesn't segfault
		content = malloc(1);
		if(content==NULL) {
			log_fatal("out of memory: malloc failed while reading XML value.");
		}
		strcpy(content, "");
	}
	

	// cleanup
	xmlXPathFreeNodeSet( nodes );
	xmlXPathFreeNodeSetList( object );
	xmlXPathFreeContext( context );

	trim(content);
	return content;
}

/*----------------------------------------------------------------------------------------
 * Purpose: return the value of a single node's attribute as a string
 * Input:	
 * Output: a trimed string
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
char *
getAttributeAsString( xmlDocPtr document, char *xpath, char *attribute )
{
	xmlXPathContextPtr context = xmlXPathNewContext( document );
	if ( context == NULL )
		return NULL;
	
	xmlXPathObjectPtr object = xmlXPathEvalExpression( (xmlChar *)xpath, context );
	if ( object == NULL )
	{
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	xmlNodeSetPtr nodes = object->nodesetval;
	if ( nodes == NULL || nodes->nodeNr != 1 )
	{
		xmlXPathFreeContext( context );
		xmlXPathFreeNodeSetList( object );
		if (nodes != NULL)
			xmlXPathFreeNodeSet( nodes );		
		return NULL;
	}
	
	xmlNodePtr element = nodes->nodeTab[0];
	if ( element->type != XML_ELEMENT_NODE )
	{
		xmlXPathFreeNodeSet( nodes );
		xmlXPathFreeNodeSetList( object );
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	char *value = (char *)xmlGetProp( element, (xmlChar *)attribute );

	// cleanup
	xmlXPathFreeNodeSet( nodes );
	xmlXPathFreeNodeSetList( object );
	xmlXPathFreeContext( context );
	
	return value;
}

/*----------------------------------------------------------------------------------------
 * Purpose: Returns a count of the nodes with the same search path.
 * Input:	
 * Output: a trimed string
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/ 
int 
getNumOfNodes( xmlDocPtr document, char *xpath )
{
	int number = 0;
	xmlXPathContextPtr context = xmlXPathNewContext( document );
	if ( context == NULL )
		return 0;
	
	xmlXPathObjectPtr object = xmlXPathEvalExpression( (xmlChar *)xpath, context );
	if ( object == NULL )
	{
		xmlXPathFreeContext( context );
		return 0;
	}
	xmlNodeSetPtr nodes = object->nodesetval;
	if ( nodes == NULL )
	{
		xmlXPathFreeContext( context );
		xmlXPathFreeNodeSetList( object );		
		return 0;	
	}
	
	number = nodes->nodeNr;
	xmlXPathFreeNodeSet( nodes );
	xmlXPathFreeNodeSetList( object );
	xmlXPathFreeContext( context );	
	return number;
}	

 /*----------------------------------------------------------------------------------------
 * Purpose:  return the particular value of a node-list as a string
 * Input:	
 * Output: a trimed string
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/ 
char *
getValueFromListAsString( xmlDocPtr document, char *xpath, char *tag, int item, int maxLength)
{
	xmlXPathContextPtr context = xmlXPathNewContext( document );
	if ( context == NULL )
		return NULL;
	
	xmlXPathObjectPtr object = xmlXPathEvalExpression( (xmlChar *)xpath, context );
	if ( object == NULL )
	{
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	xmlNodeSetPtr nodes = object->nodesetval;
	if ( nodes == NULL || item >= nodes->nodeNr )
	{
		xmlXPathFreeContext( context );
		xmlXPathFreeNodeSetList( object );
		if (nodes != NULL)
			xmlXPathFreeNodeSet( nodes );		
		return NULL;		
	}
	
	xmlNodePtr elements = nodes->nodeTab[item];
	if ( elements->type != XML_ELEMENT_NODE )
	{
		xmlXPathFreeNodeSet( nodes );
		xmlXPathFreeNodeSetList( object );
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	xmlNodePtr child = elements->children;
	while ( child != NULL && xmlStrcmp(child->name, (xmlChar *) tag) != 0 )
	{
		child = child->next;
	}
	if ( child == NULL || child->type != XML_ELEMENT_NODE )
	{
		xmlXPathFreeNodeSet( nodes );
		xmlXPathFreeNodeSetList( object );
		xmlXPathFreeContext( context );
		return NULL;
	};	
	
	xmlNodePtr text = child->children;
	if ( text->type != XML_TEXT_NODE )
	{
		xmlXPathFreeNodeSet( nodes );
		xmlXPathFreeNodeSetList( object );
		xmlXPathFreeContext( context );
		return NULL;
	}
	
	char *content = (char *)xmlNodeGetContent(text);
		
	// cleanup
	xmlXPathFreeNodeSet( nodes );
	xmlXPathFreeNodeSetList( object );
	xmlXPathFreeContext( context );
	
	trim(content);
	return content;
}


/*----------------------------------------------------------------------------------------
 * Purpose: return the value of a single node as a int between max and min
 * Input: value - a pointer to int, used to return the value
 *		document - a pointer to the XML document
 *		xpath - a x-path pointing towards the value needed
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 *
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry,
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
int getValueAsInt( int *value, xmlDocPtr document, char * xpath, int min_value, int max_value)
{
	*value = -1;
	char *temp;
	temp = (char *)getValueAsString(document, xpath, XML_MAX_CHARS);

	if( temp == NULL )
		return CONFIG_NO_ENTRY;
	else
	{
		*value = atoi(temp);
		free(temp);
		if(*value>=min_value && *value<=max_value)
			return CONFIG_VALID_ENTRY;
		else
		{
			*value = -1;
			return CONFIG_INVALID_ENTRY;
		}
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: return the value of a single node as a int between max and min
 * Input: value - a pointer to float, used to return the value
 *		document - a pointer to the XML document
 *		xpath - a x-path pointing towards the value needed
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 *
 * Output:  CONFIG_VALID_ENTRY means a valid config entry,
 *		   CONFIG_NO_ENTRY means not found config entry,,
 *		   CONFIG_INVALID_ENTRY means a invalid config entry,
 *
 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
int  getValueAsFloat( float *value, xmlDocPtr document, char * xpath, float min_value, float max_value)
{
	*value = -1;
	char *temp;
	temp = (char *)getValueAsString(document, xpath, XML_MAX_CHARS);
	if( temp == NULL )
		return CONFIG_NO_ENTRY;
	else
	{
		*value = atof(temp);
		free(temp);
		if(*value>=min_value && *value<=max_value)
			return CONFIG_VALID_ENTRY;
		else
		{
			*value = -1;
			return CONFIG_INVALID_ENTRY;
		}
	}
}


/*----------------------------------------------------------------------------------------
 * Purpose: return the particular value of a node-list as a int between max and min
 * Input: value - a pointer to int, used to return the value
 *		document - a pointer to the XML document
 *		xpath - a x-path pointing towards the node list
 *		tag -  the node's tag	
 *		item - the index of the node	
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry

 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
int getValueFromListAsInt( int *value, xmlDocPtr document, char * xpath, char *tag, int item, int min_value, int max_value)
{
	*value = -1;
	char *temp;
	temp = (char *)getValueFromListAsString(document, xpath, tag, item, XML_MAX_CHARS);
	if( temp == NULL )
		return CONFIG_NO_ENTRY;
	else
	{
		*value = atoi(temp);
		free(temp);
		
		if(*value>=min_value && *value<=max_value)
			return CONFIG_VALID_ENTRY;
		else
		{
			*value = -1;
			return CONFIG_INVALID_ENTRY;
		}
	}
}

/*----------------------------------------------------------------------------------------
 * Purpose: return the particular value of a node-list as a int between max and min
 * Input: value - a pointer to int, used to return the value
 *		document - a pointer to the XML document
 *		xpath - a x-path pointing towards the node list
 *		tag -  the node's tag	
 *		item - the index of the node	
 * 		min_value - the minimum value to be returned
 * 		max_value - the maximum value to be returned
 * Output: CONFIG_VALID_ENTRY means a valid config entry,
 *		  CONFIG_NO_ENTRY means not found config entry,
 *		  CONFIG_INVALID_ENTRY means a invalid config entry

 * He Yan @ Agu 24, 2008   
 * -------------------------------------------------------------------------------------*/
int getValueFromListAsInt32( u_int32_t *value, xmlDocPtr document, char * xpath, char *tag, int item, u_int32_t min_value, u_int32_t max_value)
{
	*value = -1;
	char *temp;
	temp = (char *)getValueFromListAsString(document, xpath, tag, item, XML_MAX_CHARS);
	if( temp == NULL )
		return CONFIG_NO_ENTRY;
	else
	{
		*value = atol(temp);
		free(temp);
		
		if(*value>=min_value && *value<=max_value)
			return CONFIG_VALID_ENTRY;
		else
		{
			*value = -1;
			return CONFIG_INVALID_ENTRY;
		}
	}
}
