#include "InputTranslator.hh"
#include "InputParserIS.hh"
#include "InputParserIS_Defs.hh"
#include "Teuchos_XMLParameterListHelpers.hpp"

#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <boost/lambda/lambda.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>

#include "errors.hh"
#include "exceptions.hh"
#include "dbc.hh"

#define  BOOST_FILESYTEM_NO_DEPRECATED
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/path.hpp"
#include "boost/format.hpp"
#include "boost/lexical_cast.hpp"

#include <xercesc/dom/DOM.hpp>
//#include <xercesc/dom/DOMNamedNodeMap.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
//#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/parsers/DOMLSParserImpl.hpp>
#include <xercesc/framework/StdOutFormatTarget.hpp>
#include <xercesc/util/OutOfMemoryException.hpp>
#include "ErrorHandler.hpp"
//#include <xercesc/dom/DOMError.hpp>
//#include <xercesc/dom/DOMErrorHandler.hpp>
//#include <xercesc/sax/ErrorHandler.hpp>

XERCES_CPP_NAMESPACE_USE


namespace Amanzi {
namespace AmanziNewInput {

/* ******************************************************************
 * Empty
 ****************************************************************** */
Teuchos::ParameterList translate(const std::string& xmlfilename, const std::string& xmlSchemafile) {

  Teuchos::ParameterList new_list;
  Teuchos::ParameterList def_list;

  // check if this is a new or old file
  // if old, create PL and send back
  // otherwise continue

  // do validation and parsing 
  // (by passing in xmlfile this moves the all of this here, 
  // we keep all the xerces and error handling out of main
  // which creates tons of clutter)
  // TODO: error handler
  XMLPlatformUtils::Initialize();
  const char* schemaFile(xmlSchemafile.c_str());
  const char* xmlFile(xmlfilename.c_str());

  XercesDOMParser *parser = new XercesDOMParser();
  parser->setExitOnFirstFatalError(true);
  parser->setValidationConstraintFatal(true);
  parser->setValidationScheme(XercesDOMParser::Val_Never);
  parser->setDoNamespaces(true);
  parser->setDoSchema(true);
  AmanziErrorHandler* errorHandler = new AmanziErrorHandler();
  parser->setErrorHandler(errorHandler); //EIB - commented out until Xerces update to handle XSD 1.1
  //parser->setExternalNoNamespaceSchemaLocation(schemaFile);
  //parser->loadGrammar(XMLString::transcode(schemaFile), Grammar::SchemaGrammarType, true);
  parser->useCachedGrammarInParse(true);
 
  bool errorsOccured = false;

  // EIB - this validation portion is basically useless until Xerces C++ implements XSD 1.1
  //       has only been implemented in Java version, no public plans to implement in C++
  //       adding in as much error checking in "get_..." sections as I can, when I can
  try {
      parser->parse(xmlfilename.c_str());
  }
  catch (const OutOfMemoryException& e) {
      std::cerr << "OutOfMemoryException" << std::endl;
      errorsOccured = true;
      Exceptions::amanzi_throw(Errors::Message("Ran out of memory while parsing the input file. Aborting."));
  }
  catch (...) {
      errorsOccured = true;
      Exceptions::amanzi_throw(Errors::Message("Errors occured while parsing the input file. Aborting."));
  }

  // go through each section, if it exist in the file, translate it 
  // to the old format
  DOMDocument *doc = parser->getDocument();

  // grab the version number attribute
  new_list.set<std::string>("Amanzi Input Format Version", get_amanzi_version(doc,def_list));

  // store the filename in the def_list for later use
  def_list.set<std::string>("xmlfilename",xmlfilename);

  // grab the simulation type structured vs. unstructured
  get_sim_type(doc, &def_list);

  // grab the mesh type
  //new_list.sublist(framework) = ...;

  // grab verbosity early
  def_list.sublist("simulation") = get_verbosity(doc);
    
  def_list.sublist("constants") = get_constants(doc, def_list);

  new_list.sublist("General Description") = get_model_description(doc, def_list);
  new_list.sublist("Mesh") = get_Mesh(doc, def_list);
  new_list.sublist("Domain").set<int>("Spatial Dimension",dimension_);
  new_list.sublist("Execution Control") = get_execution_controls(doc, &def_list);
  new_list.sublist("Phase Definitions") = get_phases(doc, def_list);
  new_list.sublist("Regions") = get_regions(doc, &def_list);
  new_list.sublist("Material Properties") = get_materials(doc, def_list);
  new_list.sublist("Initial Conditions") = get_initial_conditions(doc, def_list);
  new_list.sublist("Boundary Conditions") = get_boundary_conditions(doc, def_list);
  new_list.sublist("Sources") = get_sources(doc, def_list);
  new_list.sublist("Output") = get_output(doc, def_list);
  // hack to go back and add chemistry list (not geochemistry, for kd problem)
  if ( def_list.isSublist("Chemistry") ) {
    new_list.sublist("Chemistry") = def_list.sublist("Chemistry");
  } else { 
    new_list.sublist("Chemistry") = make_chemistry(def_list);
  }
  
  
  delete errorHandler;
  XMLPlatformUtils::Terminate();
  //def_list.print(std::cout,true,false);

  // return the completely translated input file as a parameter list
  return new_list;
}

/* ******************************************************************
* Empty
****************************************************************** */
Teuchos::ParameterList get_verbosity(DOMDocument* xmlDoc) {
    
    DOMNodeList* nodeList;
    DOMNode* nodeAttr;
    DOMNamedNodeMap* attrMap;
    char* textContent;
    
    // get execution contorls node
    nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("execution_controls"));
    Teuchos::ParameterList simPL;
    
    for (int i=0; i<nodeList->getLength(); i++) {
        DOMNode* ecNode = nodeList->item(i);
        if (DOMNode::ELEMENT_NODE == ecNode->getNodeType()) {
            //loop over children
            DOMNodeList* children = ecNode->getChildNodes();
            for (int j=0; j<children->getLength(); j++) {
                DOMNode* currentNode = children->item(j) ;
                if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
                    char* tagname = XMLString::transcode(currentNode->getNodeName());
                    if (strcmp(tagname,"verbosity")==0) {
                        attrMap = currentNode->getAttributes();
                        nodeAttr = attrMap->getNamedItem(XMLString::transcode("level"));
			if (nodeAttr) {
                          textContent = XMLString::transcode(nodeAttr->getNodeValue());
                          simPL.set<std::string>("verbosity",textContent);
			} else {
			  Errors::Message msg;
			  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing verbosity - " ;
			  msg << "level was missing or ill-formed. \n" ;
			  msg << "  Please correct and try again \n" ;
			  Exceptions::amanzi_throw(msg);
			}
                        XMLString::release(&textContent);
                    }
                }
            }
        }
    }
    return simPL;
    

}
    
/* ******************************************************************
 * Empty
 ****************************************************************** */
Teuchos::ParameterList get_constants(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNamedNodeMap *attrMap;
  DOMNode *namedNode;
  char* name;
  char* type;
  char* value;
  char* char_array;
  double time;
  Errors::Message msg;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
        std::cout << "Amanzi::InputTranslator: Getting Constants."<< std::endl;
    }
  }
  // read in new stuff
  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("definitions"));

  if (nodeList->getLength() > 0) {
    DOMNode* nodeD = nodeList->item(0);
    DOMNodeList* childern = nodeD->getChildNodes();
    for (int i=0; i<childern->getLength(); i++) {
      DOMNode* currentNode = childern->item(i) ;
      if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
        char* tagname = XMLString::transcode(currentNode->getNodeName());
	// deal with: constants, named_times, macros
        if (strcmp(tagname,"constants")==0) {
          DOMNodeList* kids = currentNode->getChildNodes();
          for (int j=0; j<kids->getLength(); j++) {
            DOMNode* currentKid = kids->item(j) ;
            if (DOMNode::ELEMENT_NODE == currentKid->getNodeType()) {
              char* kidname = XMLString::transcode(currentKid->getNodeName());
              // types: constant, time_constant, numerical_constant, area_mass_flux_constant
              if (strcmp(kidname,"constant")==0) {
	        attrMap = currentKid->getAttributes();
	        namedNode = attrMap->getNamedItem(XMLString::transcode("name"));
		if (namedNode) {
	            name = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "constant name was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
	        namedNode = attrMap->getNamedItem(XMLString::transcode("type"));
		if (namedNode) {
	          type = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "constant type for " << name << " was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
	        namedNode = attrMap->getNamedItem(XMLString::transcode("value"));
		if (namedNode) {
	          value = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "constant value for " << name << " was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
		if (strcmp(type,"time")==0) {
		  // check if time and convert to seconds - year = 365.25
		  // TODO: EIB - verify this works with spaces
		  // TODO: EIB - expect Akuna to move to no deliminator, need to test for this
		  char_array = strtok(value,";, ");
		  time = atof(char_array);
		  char_array = strtok(NULL,";,");
		  if (strcmp(char_array,"y")==0) { time = time*365.25*24.0*60.0*60.0; }
		  else if (strcmp(char_array,"d")==0) { time = time*24.0*60.0*60.0; }
		  else if (strcmp(char_array,"h")==0) { time = time*60.0*60.0; }
		} else {
		  time = atof(value);
		}
		// add to list
		Teuchos::ParameterList tmp;
		tmp.set<std::string>("type",type);
		tmp.set<double>("value",time);
		list.sublist("constants").sublist(name) = tmp;
                XMLString::release(&name);
                XMLString::release(&type);
                XMLString::release(&value);
	      } else if (strcmp(kidname,"time_constant")==0) {
	        attrMap = currentKid->getAttributes();
	        namedNode = attrMap->getNamedItem(XMLString::transcode("name"));
		if (namedNode) {
	          name = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "time_constant name was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
	        namedNode = attrMap->getNamedItem(XMLString::transcode("value"));
		if (namedNode) {
	          value = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "time_constant value for " << name << " was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
		// check if time and convert to seconds - year = 365.25
		// TODO: EIB - verify this works with spaces
		// TODO: EIB - expect Akuna to move to no deliminator, need to test for this
		char_array = strtok(value,";, ");
		time = atof(char_array);
		char_array = strtok(NULL,";,");
		if (strcmp(char_array,"y")==0) { time = time*365.25*24.0*60.0*60.0; }
		else if (strcmp(char_array,"d")==0) { time = time*24.0*60.0*60.0; }
		else if (strcmp(char_array,"h")==0) { time = time*60.0*60.0; }
		// add to list
		Teuchos::ParameterList tmp;
		tmp.set<double>("value",time);
		list.sublist("time_constants").sublist(name) = tmp;
                XMLString::release(&name);
                XMLString::release(&value);
	      } else if (strcmp(kidname,"numerical_constant")==0) {
	        attrMap = currentKid->getAttributes();
	        namedNode = attrMap->getNamedItem(XMLString::transcode("name"));
		if (namedNode) {
	          name = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "numerical_constant name was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
	        namedNode = attrMap->getNamedItem(XMLString::transcode("value"));
		if (namedNode) {
	          value = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "numerical_constant value for " << name << " was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
		// add to list
		Teuchos::ParameterList tmp;
		tmp.set<double>("value",atof(value));
		list.sublist("numerical_constant").sublist(name) = tmp;
                XMLString::release(&name);
                XMLString::release(&value);
	      } else if (strcmp(kidname,"area_mass_flux_constant")==0) {
	        attrMap = currentKid->getAttributes();
	        namedNode = attrMap->getNamedItem(XMLString::transcode("name"));
		if (namedNode) {
	          name = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "area_mass_flux_constant name was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
	        namedNode = attrMap->getNamedItem(XMLString::transcode("value"));
		if (namedNode) {
	          value = XMLString::transcode(namedNode->getNodeValue());
		} else {
		  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	 	  msg << "area_mass_flux_constant value for " << name << " was missing or ill-formed. \n  Please correct and try again \n" ;
	 	  Exceptions::amanzi_throw(msg);
		}
		// add to list
		Teuchos::ParameterList tmp;
		tmp.set<double>("value",atof(value));
		list.sublist("area_mass_flux_constant").sublist(name) = tmp;
                XMLString::release(&name);
                XMLString::release(&value);
	      }
              XMLString::release(&kidname);
	    }
	  }
	} else if (strcmp(tagname,"named_times")==0) {
	  //TODO: EIB - deal with named times
          DOMNodeList* kids = currentNode->getChildNodes();
          for (int j=0; j<kids->getLength(); j++) {
            DOMNode* currentKid = kids->item(j) ;
            if (DOMNode::ELEMENT_NODE == currentKid->getNodeType()) {
              char* kidname = XMLString::transcode(currentKid->getNodeName());
              // types: time
              if (strcmp(kidname,"time")==0) {
	      }
              XMLString::release(&kidname);
	    }
	  }
	//} else if (strcmp(tagname,"macros")==0) {
	  //TODO: EIB - move macros here from outputs
        }
        XMLString::release(&tagname);
      }
    }
  }

  return list;
  
}




/* ******************************************************************
 * Empty
 ****************************************************************** */
std::string get_amanzi_version(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {
  std::stringstream old_version;
  
  XMLCh* tag = XMLString::transcode("amanzi_input");

  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(tag);  
  XMLString::release(&tag);

  const XMLSize_t nodeCount = nodeList->getLength();  
  if (nodeList->getLength() > 0) {
    DOMNode* nodeGD = nodeList->item(0);
    DOMElement* elementGD = static_cast<DOMElement*>(nodeGD);
    std::string version(XMLString::transcode(elementGD->getAttribute(XMLString::transcode("version"))));
    
    int major, minor, micro;
    
    std::stringstream ss;
    ss << version;
    std::string ver;
    
    try {
      getline(ss,ver,'.');
      major = boost::lexical_cast<int>(ver);
      
      getline(ss,ver,'.');
      minor = boost::lexical_cast<int>(ver);
      
      getline(ss,ver);
      micro = boost::lexical_cast<int>(ver);
    }
    catch (...) {
      Exceptions::amanzi_throw(Errors::Message("The version string in the input file '"+version+"' has the wrong format, please use X.Y.Z, where X, Y, and Z are integers."));
    }

    if ( (major == 2) && (minor == 0) && (micro == 0) ) {
      // now we can proceed, we translate to a v1.2.1 parameterlist
      old_version << AMANZI_OLD_INPUT_VERSION_MAJOR <<"."<< AMANZI_OLD_INPUT_VERSION_MINOR <<"."<< AMANZI_OLD_INPUT_VERSION_MICRO; 
                  // "1.2.0";
    } else {
      std::stringstream ver;
      ver << AMANZI_INPUT_VERSION_MAJOR << "." << AMANZI_INPUT_VERSION_MINOR << "." << AMANZI_INPUT_VERSION_MICRO;      
      Exceptions::amanzi_throw(Errors::Message("The input version " + version + " specified in the input file is not supported. This version of amanzi supports version "+ ver.str() + "."));
    }
  } else {
    // amanzi inpurt description did not exist, error
    Exceptions::amanzi_throw(Errors::Message("Amanzi input description does not exist <amanzi_input version=...>"));
  }

  return old_version.str();
}

/* ******************************************************************
 * Empty
 ****************************************************************** */
void get_sim_type(DOMDocument* xmlDoc, Teuchos::ParameterList* def_list) {
  std::stringstream old_version;
  
  Errors::Message msg;

  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("amanzi_input"));  

  const XMLSize_t nodeCount = nodeList->getLength();  
  if (nodeList->getLength() > 0) {
    DOMNode* nodeGD = nodeList->item(0);
    DOMElement* elementGD = static_cast<DOMElement*>(nodeGD);
    std::string sim_type = (XMLString::transcode(elementGD->getAttribute(XMLString::transcode("type"))));
    if (sim_type.length() > 0) {
      def_list->set<std::string>("sim_type",sim_type);
      if (strcmp(sim_type.c_str(),"structured")==0) {
	  isUnstr_ = false;
      } else {
	  isUnstr_ = true;
      }
    } else {
      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing amanzi_input - " ;
      msg << "type was missing or ill-formed. \n  Please correct and try again \n" ;
      Exceptions::amanzi_throw(msg);
    }
    
  } else {
    // amanzi inpurt description did not exist, error
    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing amanzi_input - " ;
    msg << "type was missing or ill-formed. \n  Please correct and try again \n" ;
    Exceptions::amanzi_throw(msg);
  }

}



/* ******************************************************************
 * Empty
 ****************************************************************** */
Teuchos::ParameterList get_model_description(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
        std::cout << "Amanzi::InputTranslator: Getting Model Description."<< std::endl;
    }
  }
  // read in new stuff
  XMLCh* tag = XMLString::transcode("model_description");
  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(tag);
  XMLString::release(&tag);

  // write to old format, mostly won't be read so just write out as strings
  const XMLSize_t nodeCount = nodeList->getLength() ;
  if (nodeList->getLength() > 0) {
    DOMNode* nodeGD = nodeList->item(0);
    DOMElement* elementGD = static_cast<DOMElement*>(nodeGD);
    char* model_name = XMLString::transcode(elementGD->getAttribute(XMLString::transcode("name")));
    list.set<std::string>("model_name",model_name);

    DOMNodeList* childern = nodeGD->getChildNodes();
    for (int i=0; i<childern->getLength(); i++) {
      DOMNode* currentNode = childern->item(i) ;
      if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
        char* tagname = XMLString::transcode(currentNode->getNodeName());
        DOMNode::NodeType type = currentNode->getNodeType();
        if (strcmp(tagname,"units")!=0) {
          char* textContent = XMLString::transcode(currentNode->getTextContent());
          list.set<std::string>(tagname,textContent);
          XMLString::release(&textContent);
        }
        XMLString::release(&tagname);
        }
    }
  }
  else {
	  // model_description didn't exist, report an error
  }

  return list;
  
}

/* ******************************************************************
 * Empty
 ****************************************************************** */
Teuchos::ParameterList get_Mesh(DOMDocument* xmlDoc, Teuchos::ParameterList def_list ) {

  //TODO: EIB - see if mesh list ending up in right order/structure

  Teuchos::ParameterList list;

  bool generate = true;
  bool read = false;
  char *framework;
  Teuchos::ParameterList mesh_list;
  bool all_good = false;
  Errors::Message msg;
  std::stringstream helper;
    
  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
        std::cout << "Amanzi::InputTranslator: Getting Mesh."<< std::endl;
    }
  }

  // read in new stuff
  XMLCh* tag = XMLString::transcode("mesh");
  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(tag);
  XMLString::release(&tag);

  // read the attribute to set the framework sublist
  const XMLSize_t nodeCount = nodeList->getLength() ;
  if (nodeList->getLength() > 0) {
    DOMNode* nodeMesh = nodeList->item(0);
    DOMElement* elementMesh = static_cast<DOMElement*>(nodeMesh);
    // if unstructure, look for framework attribute
    if (isUnstr_) { 
      if(elementMesh->hasAttribute(XMLString::transcode("framework"))) {
        framework = XMLString::transcode(elementMesh->getAttribute(XMLString::transcode("framework")));
      } else { 
        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
        msg << "framework was missing or ill-formed. \n  Use default framework='mstk' if unsure.  Please correct and try again \n" ;
        Exceptions::amanzi_throw(msg);
      }
    }

    // loop over child nodes
    DOMNodeList* children = nodeMesh->getChildNodes();
    // first figure out what the dimension is
    all_good = false;
    for (int i=0; i<children->getLength(); i++) {
      DOMNode* currentNode = children->item(i) ;
      if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
	char* tagname = XMLString::transcode(currentNode->getNodeName());
	if (strcmp(tagname,"dimension")==0) {
	  char* temp = XMLString::transcode(currentNode->getTextContent());
	  if (strlen(temp) > 0) {
	      dimension_ = get_int_constant(temp,def_list);
	      all_good = true;
	  }
	}
      }
    }
    if (!all_good) {
        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
        msg << "dimension was missing or ill-formed. \n  Please correct and try again \n" ;
        Exceptions::amanzi_throw(msg);
    }

    // now we can properly parse the generate/read list
    all_good = false;
    for (int i=0; i<children->getLength(); i++) {
      DOMNode* currentNode = children->item(i) ;
      if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
	char* tagname = XMLString::transcode(currentNode->getNodeName());   

	if (strcmp(tagname,"generate")==0) {
	  all_good = true;
	  generate = true;
	  read = false;
	  DOMElement* elementGen = static_cast<DOMElement*>(currentNode);

	  // get Number of Cells
	  Teuchos::Array<int> ncells; 
	  DOMNodeList* nodeList = elementGen->getElementsByTagName( XMLString::transcode("number_of_cells"));
	  DOMNode* node = nodeList->item(0);
	  DOMElement* elementNode = static_cast<DOMElement*>(node);
	  DOMNamedNodeMap *attrMap = node->getAttributes();
          DOMNode* nodeAttr;
          char* attrName;
	  char* temp;
	  // make sure number of attributes equals dimension
	  if ( attrMap->getLength() == dimension_) {
	    // loop over attributes to get nx, ny, nz as needed
	    for (int j=0; j<attrMap->getLength(); j++) {
	      nodeAttr = attrMap->item(j);
	      attrName =XMLString::transcode(nodeAttr->getNodeName());
	      if (attrName) {
	        temp = XMLString::transcode(nodeAttr->getNodeValue());
	        if (strlen(temp) > 0) {
	        ncells.append(get_int_constant(temp,def_list));
		} else {
		  all_good = false;
	          helper << "  -> number_of_cells "<<attrName<<" ill-formed or missing\n";
	        }
	      } else {
		all_good = false;
	        helper << "  -> number_of_cells "<<attrName<<" ill-formed or missing\n";
	      }
	      XMLString::release(&temp);
	    }
            mesh_list.set<Teuchos::Array<int> >("Number of Cells",ncells);
	  } else {
	    helper << "  -> number_of_cells does not match dimension\n";
	    all_good = false;
	  }

	  // get Box - generalize
	  char* char_array;
	  nodeList = elementGen->getElementsByTagName( XMLString::transcode("box"));
	  node = nodeList->item(0);
	  elementNode = static_cast<DOMElement*>(node);
	  temp = XMLString::transcode(elementNode->getAttribute( XMLString::transcode("low_coordinates")));
	  if (strlen(temp) > 0) {
	    // translate to array
	    Teuchos::Array<double> low = make_coordinates(temp, def_list);
            mesh_list.set<Teuchos::Array<double> >("Domain Low Corner",low);
	    if (low.length() != dimension_) {
	      helper << "  -> low_coordinates ill-formed or missing\n";
	      all_good = false;
	    }
	  } else {
	    helper << "  -> low_coordinates ill-formed or missing\n";
	    all_good = false;
	  }
	  XMLString::release(&temp);
	  temp = XMLString::transcode(elementNode->getAttribute( XMLString::transcode("high_coordinates")));
	  if (strlen(temp) > 0) {
	    // translate to array
	    Teuchos::Array<double> high = make_coordinates(temp, def_list);
            mesh_list.set<Teuchos::Array<double> >("Domain High Corner",high);
	    if (high.length() != dimension_) {
	      helper << "  -> high_coordinates ill-formed or missing\n";
	      all_good = false;
	    }
	  } else {
	    helper << "  -> high_coordinates ill-formed or missing\n";
	    all_good = false;
	  }
	  XMLString::release(&temp);
	}

	else if (strcmp(tagname,"read")==0) {
	  read = true;
	  generate = false;
	  bool goodtype = false;
	  bool goodname = false;
	  DOMElement* elementRead = static_cast<DOMElement*>(currentNode);

	  char* format = XMLString::transcode(elementRead->getElementsByTagName(
				  XMLString::transcode("format"))->item(0)->getTextContent());
          if (strcmp(format,"exodus ii")==0 || strcmp(format,"exodus II")==0 || 
	      strcmp(format,"Exodus II")==0 || strcmp(format,"Exodus ii")==0) {
	      mesh_list.set<std::string>("Format","Exodus II");
	      goodtype = true;
	  }
          else if (strcmp(format,"h5m") == 0 || strcmp(format,"H5M") == 0) {
            mesh_list.set<std::string>("Format","H5M");
            goodtype = true;
          }
	  char* filename = XMLString::transcode(elementRead->getElementsByTagName(
				  XMLString::transcode("file"))->item(0)->getTextContent());
	  if (strlen(filename) > 0) {
              mesh_list.set<std::string>("File",filename);
	      goodname = true;
	  }
	  XMLString::release(&format);
	  XMLString::release(&filename);
	  if (goodtype && goodname) all_good = true;

	}
	//EIB - handles legacy files
	//TODO:: EIB - remove to conform to updated schema
	else if (strcmp(tagname,"file")==0) {
	  read = true;
	  generate = false;
	  char* filename = XMLString::transcode(currentNode->getTextContent());
	  if (strlen(filename) > 0) {
            mesh_list.set<std::string>("File",filename);
	    mesh_list.set<std::string>("Format","Exodus II");
	    all_good = true;
	    std::cout << "Amanzi::InputTranslator: Warning - " << std::endl;
	    std::cout << "    Please note - the XML Schema for specifing a mesh file to read has been updated." << std::endl;
	    std::cout << "    See the Amanzi User Guide for the latest information." << std::endl;
	    std::cout <<"     The legacy format is being handled for now.  Please update input files for future versions." << std::endl;
	  }
	  XMLString::release(&filename);
	}

      }
    }
    if (!all_good) {
        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
        msg << "generate/read was missing or ill-formed. \n  Please correct and try again \n" ;
	msg << helper.str();
        Exceptions::amanzi_throw(msg);
    }
    
    if (generate || read) {
      if (isUnstr_) {
        if (strcmp(framework,"mstk")==0) {
          list.sublist("Unstructured").sublist("Expert").set<std::string>("Framework","MSTK");
        } else if (strcmp(framework,"moab")==0) {
          list.sublist("Unstructured").sublist("Expert").set<std::string>("Framework","MOAB");
        } else if (strcmp(framework,"simple")==0) {
          list.sublist("Unstructured").sublist("Expert").set<std::string>("Framework","Simple");
        } else if (strcmp(framework,"stk::mesh")==0) {
          list.sublist("Unstructured").sublist("Expert").set<std::string>("Framework","stk::mesh");
        } else {
          //list.sublist("Unstructured").sublist("Expert").set<std::string>("Framework","MSTK");
          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
          msg << "unknown framework=" << framework << ". See the schema for acceptable types. \n  Please correct and try again \n" ;
          Exceptions::amanzi_throw(msg);
	}
      }
    }

    if (generate) {
      if (isUnstr_) {
        list.sublist("Unstructured").sublist("Generate Mesh").sublist("Uniform Structured") = mesh_list;
      } else {
        list.sublist("Structured") = mesh_list;
      }
    }
    else if (read) {
      if (isUnstr_) {
	list.sublist("Unstructured").sublist("Read Mesh File") = mesh_list;
      }
    }
    else {
      // bad mesh, again if validated shouldn't need this
      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
      msg << "\n  Please correct and try again \n" ;
      Exceptions::amanzi_throw(msg);
    }
  }
  else {
    // no mesh sub-elements
    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing mesh - " ;
    msg << "framework was missing or ill-formed. \n  Please correct and try again \n" ;
    Exceptions::amanzi_throw(msg);
  }

  XMLString::release(&framework);

  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_execution_controls(DOMDocument* xmlDoc, Teuchos::ParameterList* def_list ) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNode* nodeTmp;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* attrName;
  char* textContent;
  char* elemContent;
  std::string meshbase;
  Errors::Message msg;

  // This actually includes: process kernels, execution controls, and numerical controls
  // all three map back to the old exection controls
  
  if (isUnstr_) {
    meshbase = std::string("Unstructured Algorithm");
  } else {
    meshbase = std::string("Structured Algorithm");
  }

    // set so we don't have to reread transport and chemisty list later
  Teuchos::ParameterList fpkPL, tpkPL, cpkPL;
  bool flowON=false;
  bool staticflowON=false;
  bool transportON=false;
  bool chemistryON=false;

  // get process kernels node
  if (def_list->sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Process Kernels."<< std::endl;
    }
  }
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("process_kernels"));
  for (int i=0; i<nodeList->getLength(); i++) {
    DOMNode* pkNode = nodeList->item(i);
    if (DOMNode::ELEMENT_NODE == pkNode->getNodeType()) {
      DOMElement* pkElement = static_cast<DOMElement*>(pkNode);
      // get flow
      DOMNodeList* tmpList = pkElement->getElementsByTagName(XMLString::transcode("flow"));
      if (tmpList->getLength() > 0 ) {
        DOMElement* flowElement = static_cast<DOMElement*>(tmpList->item(0));
        if (flowElement->hasAttribute((XMLString::transcode("state")))) {
	  textContent = XMLString::transcode(flowElement->getAttribute(XMLString::transcode("state")));
          if (strcmp(textContent,"off")==0){
              list.set<std::string>("Flow Model","Off");
          } else if (strcmp(textContent,"on")==0) {
	      flowON = true;
	      char* textContent2 = XMLString::transcode(flowElement->getAttribute(XMLString::transcode("model")));
	      if (strcmp(textContent2,"saturated")==0) {
                  list.set<std::string>("Flow Model","Single Phase");
	      } else if (strcmp(textContent2,"richards")==0) {
                  list.set<std::string>("Flow Model","Richards");
              } else if (strcmp(textContent2,"constant")==0) {
		  // EIB - also need to set integration mode = transient with static flow
                  list.set<std::string>("Flow Model","Single Phase");
		  staticflowON = true;
              } else { 
                  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels->flow - " ;
                  msg << "model was missing or ill-formed. \n  Please correct and try again \n" ;
                  Exceptions::amanzi_throw(msg);
              }
              XMLString::release(&textContent2);
          } else { 
            msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels->flow - " ;
            msg << "state was missing or ill-formed. \n  Please correct and try again \n" ;
            Exceptions::amanzi_throw(msg);
          }
          XMLString::release(&textContent);
        } else { 
          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels->flow - " ;
          msg << "state was missing or ill-formed. \n  Please correct and try again \n" ;
          Exceptions::amanzi_throw(msg);
        }
        // EIB: stubbing in for feature in spec, but not yet in schema
        if (flowElement->hasAttribute((XMLString::transcode("discretization_method")))) {
	      textContent = XMLString::transcode(
	              flowElement->getAttribute(XMLString::transcode("discretization_method")));
	      fpkPL.set<std::string>("Discretization Method",textContent);
          XMLString::release(&textContent);
        }
        // EIB: stubbing in for feature in spec, but not yet in schema
        if (flowElement->hasAttribute((XMLString::transcode("relative_permeability")))) {
	      textContent = XMLString::transcode(
	              flowElement->getAttribute(XMLString::transcode("relative_permeability")));
	      fpkPL.set<std::string>("Relative Permeability",textContent);
          XMLString::release(&textContent);
        }
        // EIB: stubbing in for feature in spec, but not yet in schema
        if (flowElement->hasAttribute((XMLString::transcode("atmospheric_pressure")))) {
	      textContent = XMLString::transcode(
	              flowElement->getAttribute(XMLString::transcode("atmospheric_pressure")));
	      fpkPL.set<double>("atmospheric pressure",get_double_constant(textContent,*def_list));
          XMLString::release(&textContent);
        }
      } else { 
        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels - " ;
        msg << "flow was missing or ill-formed. \n  Please correct and try again \n" ;
        Exceptions::amanzi_throw(msg);
      }

      // get transport
      tmpList = pkElement->getElementsByTagName(XMLString::transcode("transport"));
      DOMElement* transElement = static_cast<DOMElement*>(tmpList->item(0));
      if (transElement->hasAttribute((XMLString::transcode("state")))) {
	      textContent = XMLString::transcode(
	              transElement->getAttribute(XMLString::transcode("state")));
          if (strcmp(textContent,"off")==0) {
              list.set<std::string>("Transport Model","Off");
          } else {
              list.set<std::string>("Transport Model","On");
  	          transportON=true;
	      }
          XMLString::release(&textContent);
      }
      if (transElement->hasAttribute((XMLString::transcode("algorithm")))) {
	      textContent = XMLString::transcode(
	              transElement->getAttribute(XMLString::transcode("algorithm")));
	      if (strcmp(textContent,"explicit first-order")==0) {
              tpkPL.set<std::string>("Transport Integration Algorithm","Explicit First-Order");
	      } else if (strcmp(textContent,"explicit second-order")==0) {
	          tpkPL.set<std::string>("Transport Integration Algorithm","Explicit Second-Order");
	      } else if (strcmp(textContent,"none")==0) {
	          tpkPL.set<std::string>("Transport Integration Algorithm","None");
	      }
          XMLString::release(&textContent);
      }
      if (transElement->hasAttribute((XMLString::transcode("sub_cycling")))) {
     	  textContent = XMLString::transcode(
	              transElement->getAttribute(XMLString::transcode("sub_cycling")));
	      if (strcmp(textContent,"on")==0) {
	          tpkPL.set<bool>("transport subcycling",true);
	      } else  {
	          tpkPL.set<bool>("transport subcycling",false);
	      }
          XMLString::release(&textContent);
      }
      // EIB: stubbing in for feature in spec, but not yet in schema
      if (transElement->hasAttribute((XMLString::transcode("cfl")))) {
	       textContent = XMLString::transcode(
	              transElement->getAttribute(XMLString::transcode("cfl")));
	      tpkPL.set<double>("CFL",atof(textContent));
          XMLString::release(&textContent);
      }

      // get chemisty - TODO: EIB - assuming this will be set to OFF!!!!!
      // NOTE: EIB - old spec options seem to be ON/OFF, algorithm option goes under Numerical Control Parameters
      tmpList = pkElement->getElementsByTagName(XMLString::transcode("chemistry"));
      attrMap = tmpList->item(0)->getAttributes();
      nodeAttr = attrMap->getNamedItem(XMLString::transcode("state"));
      if (nodeAttr) {
        attrName = XMLString::transcode(nodeAttr->getNodeValue());
      } else {
	Errors::Message msg;
	msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels - " ;
	msg << "No attribute state found for chemistry. \n" ;
	msg << "  Please correct and try again \n" ;
	Exceptions::amanzi_throw(msg);
      }
      if (strcmp(attrName,"off")==0) {
        list.set<std::string>("Chemistry Model","Off");
      } else {
	// EIB - this is no longer valid
        //list.set<std::string>("Chemistry Model","On");
    	chemistryON=true;
    	//TODO: EIB - now get chemistry engine option
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("engine"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
        } else {
	  Errors::Message msg;
 	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels - " ;
	  msg << "No attribute engine found for chemistry. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
        }

	if (strcmp(textContent,"amanzi")==0) {
          list.set<std::string>("Chemistry Model","Amanzi");
	} else if (strcmp(textContent,"pflotran")==0) { 
	  // TODO: EIB - should this be pflotran or alquimia????
          list.set<std::string>("Chemistry Model","Alquimia");
	} else {
		//TODO: EIB - error handle here!!!
	}
        XMLString::release(&textContent);
    	//TODO: EIB - now get process model option
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("process_model"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
        } else {
	  Errors::Message msg;
 	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing process_kernels - " ;
	  msg << "No attribute process_model found for chemistry. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
        }

	if (strcmp(textContent,"implicit operator split")==0) {
	    //cpkPL.set<double>("max chemistry to transport timestep ratio",get_double_constant(textContent,*def_list));
	}
        XMLString::release(&textContent);
      }
      XMLString::release(&attrName);
    }
  }
 
  if (def_list->sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Execution Controls."<< std::endl;
    }
  }

  // get execution contorls node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("execution_controls"));
  Teuchos::ParameterList ecsPL;
  Teuchos::ParameterList defPL;
  bool hasSteady = false;
  bool hasTrans = false;
  bool hasRestart = false;
  int numControlPeriods = 0;
  Teuchos::Array<double> start_times;
  double sim_start=-1.;
  double sim_end=-1.;
  Teuchos::ParameterList simPL;

  for (int i=0; i<nodeList->getLength(); i++) {
    DOMNode* ecNode = nodeList->item(i);
    if (DOMNode::ELEMENT_NODE == ecNode->getNodeType()) {
      //loop over children
      DOMNodeList* children = ecNode->getChildNodes();
      for (int j=0; j<children->getLength(); j++) {
        DOMNode* currentNode = children->item(j) ;
        if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	  char* tagname = XMLString::transcode(currentNode->getNodeName());
          if (strcmp(tagname,"verbosity")==0) {
              attrMap = currentNode->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("level"));
	      if (nodeAttr) {
                textContent = XMLString::transcode(nodeAttr->getNodeValue());
              } else {
	        Errors::Message msg;
 	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing execution_controls - " ;
	        msg << "No attribute level found for verbosity. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
              }
              list.set<std::string>("Verbosity",textContent);
              simPL.set<std::string>("verbosity",textContent);
              XMLString::release(&textContent);

	  } else if (strcmp(tagname,"execution_control_defaults")==0) {
              attrMap = currentNode->getAttributes();
              for (int k=0; k<attrMap->getLength(); k++) {
		nodeAttr = attrMap->item(k);
		attrName =XMLString::transcode(nodeAttr->getNodeName());
		textContent = XMLString::transcode(nodeAttr->getNodeValue());
		defPL.set<std::string>(attrName,textContent);
		// EIB:: only include default mode if > 1 EC
		//if (strcmp(attrName,"mode")==0) {
		//  if (strcmp(textContent,"steady")==0) {
		//    hasSteady = true;
		//  } else {
		//    hasTrans = true;
		//  }
		//}
	      }
	  } else if (strcmp(tagname,"execution_control")==0) {
              Teuchos::ParameterList ecPL;
	      numControlPeriods++;
              attrMap = currentNode->getAttributes();
	      char* name;
	      bool saveName=true;
              for (int k=0; k<attrMap->getLength(); k++) {
		nodeAttr = attrMap->item(k);
		attrName =XMLString::transcode(nodeAttr->getNodeName());
		textContent = XMLString::transcode(nodeAttr->getNodeValue());
		ecPL.set<std::string>(attrName,textContent);
		if (strcmp(attrName,"start")==0) {
		  if (saveName) name=textContent;
		  double time = get_time_value(textContent, *def_list);
		  if (start_times.length() == 0) {  // if first time through
		    start_times.append(time); 
		    sim_start = time;
		  } else {
		    if (time < sim_start) sim_start = time;           // check for simulation start time
		    if (time >= start_times[start_times.length()-1]) { // if already sorted
		      start_times.append(time);
		    } else {                                          // else, sort
		      int idx = start_times.length()-1;
		      Teuchos::Array<double> hold;
		      hold.append(start_times[idx]);
		      start_times.remove(idx);
		      idx--;
		      while (time < start_times[idx]) {
		        hold.append(start_times[idx]);
		        start_times.remove(idx);
		        idx--;
		      }
		      start_times.append(time);
		      for (int i=0; i<hold.length(); i++) {
		        idx = hold.length()-1-i;
		        start_times.append(hold[hold.length()-idx]);
		      }
		    }
		  }
		}
		if (strcmp(attrName,"end")==0) {
		  double time = get_time_value(textContent, *def_list);
		  if (time > sim_end) sim_end = time;                   // check for simulation end time
		}
		if (strcmp(attrName,"mode")==0) {
		  if (strcmp(textContent,"steady")==0) {
		    hasSteady = true;
		    saveName = false;
		    name = textContent;
		  } else {
		    hasTrans = true;
		  }
		}
		if (strcmp(attrName,"restart")==0) {
		    hasRestart = true;
		    //name = attrName;
	            //ecsPL.sublist("restart") = ecPL;
		}
	      }
	      if (hasRestart && ecPL.isParameter("restart")) ecsPL.sublist("restart") = ecPL;
	      ecsPL.sublist(name) = ecPL;
	  }
	}
      }
    }
  }
  // do an end time check 
  if (sim_end == -1.) {
    sim_end = start_times[start_times.length()-1];
  }
  simPL.set<double>("simulation_start",sim_start);
  simPL.set<double>("simulation_end",sim_end);
  simPL.set<Teuchos::Array<double> >("simulation_start_times",start_times);
  //ecsPL.sublist("simulation") = simPL;
  def_list->sublist("simulation") = simPL;

  // If > 1 EC, then include default mode || mode wasn't set in any execution control statement
  if (numControlPeriods > 1 || (!hasTrans && !hasSteady)) {
    if (defPL.isParameter("mode")) {
	if (defPL.get<std::string>("mode") == "steady") hasSteady = true;
	if (defPL.get<std::string>("mode") == "transient") hasTrans = true;
    }
  }

  // Now, go back and sort things out
  bool haveSSF = false; // have steady-steady incr/red factors for later
  bool haveTF = false;  // have transient incr/red factors for later
  
  // Restart
  if (hasRestart) {
      std::string value = ecsPL.sublist("restart").get<std::string>("restart");
      Teuchos::ParameterList restartPL;
      restartPL.set<std::string>("Checkpoint Data File Name",value);
      list.sublist("Restart from Checkpoint Data File") = restartPL;
  }

  // Steady case
  if (hasSteady && !hasTrans) {
    if (def_list->sublist("simulation").isParameter("verbosity")) {
      std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
      if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Creating Steady State Execution Control."<< std::endl;
      }
    }
    // NOTE:: if you edit the Steady case, it is repeated under the Initialize to Steady case, so edit there too!!!!
    Teuchos::ParameterList steadyPL;
    // look for values from default list
    // if not there, grab from ec list
    std::string value;
    std::string method;
    bool gotValue;
    if (defPL.isParameter("start")) {
      value = defPL.get<std::string>("start");
      gotValue = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("start")) {
            value = ecsPL.sublist(it->first).get<std::string>("start");
            gotValue = true;
	  }
	}
      }
    }
    if (gotValue) {
      double time = get_time_value(value, *def_list);
      steadyPL.set<double>("Start",time);
      gotValue = false;
    } else {
      // ERROR - for unstructured, optional for structured;
    }
    if (defPL.isParameter("end")) {
      value = defPL.get<std::string>("end");
      gotValue = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("end")) {
            value = ecsPL.sublist(it->first).get<std::string>("end");
            gotValue = true;
	  }
	}
      }
    }
    if (gotValue) {
      double time = get_time_value(value, *def_list);
      steadyPL.set<double>("End",time);
      gotValue = false;
    } else {
      // ERROR ;
    }
    if (defPL.isParameter("init_dt")) {
      value = defPL.get<std::string>("init_dt");
      gotValue = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("init_dt")) {
            value = ecsPL.sublist(it->first).get<std::string>("init_dt");
            gotValue = true;
	  }
	}
      }
    }
    if (gotValue) {
      steadyPL.set<double>("Initial Time Step",get_time_value(value,*def_list));
      gotValue = false;
    } else {
      // default value to 0.0
      steadyPL.set<double>("Initial Time Step",0.0);
    }
    if (defPL.isParameter("method")) {
      method = defPL.get<std::string>("method");
      gotValue = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("method")) {
            method = ecsPL.sublist(it->first).get<std::string>("method");
            gotValue = true;
	  }
	}
      }
    }
    if (gotValue && strcmp(method.c_str(),"picard")==0) {
      steadyPL.set<bool>("Use Picard","true");
      gotValue = false;
    } else {
      // ERROR ;
    }
    if (defPL.isParameter("reduction_factor")) {
      haveSSF = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("reduction_factor")) {
            value = ecsPL.sublist(it->first).get<std::string>("reduction_factor");
            haveSSF = true;
	  }
	}
      }
    }
    if (defPL.isParameter("increase_factor")) {
      haveSSF = true;
    } else {
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	if (it->first != "restart") {
          if (ecsPL.sublist(it->first).isParameter("increase_factor")) {
            haveSSF = true;
	  }
	}
      }
    }
    list.sublist("Time Integration Mode").sublist("Steady") = steadyPL;

  } else {
    if (!hasSteady) {
    // Transient case
    if (def_list->sublist("simulation").isParameter("verbosity")) {
      std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
      if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Creating Transient Execution Control."<< std::endl;
      }
    }
      Teuchos::ParameterList transPL;
      // loop over ecs to set up, TPC lists
      Teuchos::Array<double> start_times;
      Teuchos::Array<double> init_steps;
      Teuchos::Array<double> max_steps;
      for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	// skip this list, if labeled "restart" it is a duplicate list
	if (it->first != "restart") {
	  bool gotValue;
	  std::string Value;
	  if (ecsPL.sublist(it->first).isParameter("start")) {
            Value = ecsPL.sublist(it->first).get<std::string>("start");
            gotValue = true;
	  } else {
            Value = defPL.get<std::string>("start");
            gotValue = true;
	  }
          if (gotValue) {
	    double time = get_time_value(Value,*def_list);
	    start_times.append(time);
	    gotValue = false;
	  }
	  if (ecsPL.sublist(it->first).isParameter("end")) {
            Value = ecsPL.sublist(it->first).get<std::string>("end");
            gotValue = true;
	  } else {
            Value = defPL.get<std::string>("end");
            gotValue = true;
	  }
          if (gotValue) {
	    double time = get_time_value(Value,*def_list);
	    transPL.set<double>("End",time);
	    gotValue = false;
	  }
	  if (ecsPL.sublist(it->first).isParameter("init_dt")) {
            Value = ecsPL.sublist(it->first).get<std::string>("init_dt");
            gotValue = true;
	  } else {
	    if (defPL.isParameter("init_dt")) {
              Value = defPL.get<std::string>("init_dt");
              gotValue = true;
	    } 
	  }
          if (gotValue) {
	    init_steps.append(get_time_value(Value,*def_list));
	    gotValue = false;
	  }
	  if (ecsPL.sublist(it->first).isParameter("max_dt")) {
            Value = ecsPL.sublist(it->first).get<std::string>("max_dt");
            gotValue = true;
	  } else {
	    if (defPL.isParameter("max_dt")) {
              Value = defPL.get<std::string>("max_dt");
              gotValue = true;
	    }
	  }
          if (gotValue) {
	    max_steps.append(get_time_value(Value,*def_list));
	    gotValue = false;
	  }
	  if (ecsPL.sublist(it->first).isParameter("reduction_factor") || defPL.isParameter("reduction_factor")) {
            haveTF = true;
          }
	  if (ecsPL.sublist(it->first).isParameter("increase_factor") || defPL.isParameter("increase_factor")) {
            haveTF = true;
          }
        }
      }
      transPL.set<double>("Start",start_times[0]);
      if ( init_steps.length() > 0 ) transPL.set<double>("Initial Time Step",init_steps[0]);
      if ( max_steps.length() > 0 ) transPL.set<double>("Maximum Time Step Size",max_steps[0]);
      if  (!staticflowON) {
        list.sublist("Time Integration Mode").sublist("Transient") = transPL;
      } else {
        list.sublist("Time Integration Mode").sublist("Transient with Static Flow") = transPL;
      }
      if (start_times.length() > 1) {
	// to include "Time Period Control" list
        Teuchos::ParameterList tpcPL;
	tpcPL.set<Teuchos::Array<double> >("Start Times",start_times);
	if ( init_steps.length() > 0 ) tpcPL.set<Teuchos::Array<double> >("Initial Time Step",init_steps);
	if ( max_steps.length() > 0 ) tpcPL.set<Teuchos::Array<double> >("Maximum Time Step",max_steps);
	list.sublist("Time Period Control") = tpcPL;
      }
    } else {
    // Initialize to Steady case
      if (numControlPeriods==1) {
        // user screwed up and really meant Steady case
        if (def_list->sublist("simulation").isParameter("verbosity")) {
          std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
          if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Creating Steady State Execution Control."<< std::endl;
          }
        }
	Teuchos::ParameterList steadyPL;
        // look for values from default list
        // if not there, grab from ec list
        std::string value;
        std::string method;
        bool gotValue;
        if (defPL.isParameter("start")) {
          value = defPL.get<std::string>("start");
          gotValue = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("start")) {
                value = ecsPL.sublist(it->first).get<std::string>("start");
                gotValue = true;
	      }
	    }
          }
        }
        if (gotValue) {
          double time = get_time_value(value, *def_list);
          steadyPL.set<double>("Start",time);
          gotValue = false;
        } else {
          // ERROR - for unstructured, optional for structured;
        }
        if (defPL.isParameter("end")) {
          value = defPL.get<std::string>("end");
          gotValue = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("end")) {
                value = ecsPL.sublist(it->first).get<std::string>("end");
                gotValue = true;
	      }
	    }
          }
        }
        if (gotValue) {
          double time = get_time_value(value, *def_list);
          steadyPL.set<double>("End",time);
          gotValue = false;
        } else {
          // ERROR ;
        }
        if (defPL.isParameter("init_dt")) {
          value = defPL.get<std::string>("init_dt");
          gotValue = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("init_dt")) {
                value = ecsPL.sublist(it->first).get<std::string>("init_dt");
                gotValue = true;
	      }
	    }
          }
        }
        if (gotValue) {
          steadyPL.set<double>("Initial Time Step",get_time_value(value,*def_list));
          gotValue = false;
        } else {
          // default value to 0.0
          steadyPL.set<double>("Initial Time Step",0.0);
        }
        if (defPL.isParameter("method")) {
          method = defPL.get<std::string>("method");
          gotValue = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("method")) {
                method = ecsPL.sublist(it->first).get<std::string>("method");
                gotValue = true;
	      }
	    }
          }
        }
        if (gotValue && strcmp(method.c_str(),"picard")==0) {
          steadyPL.set<bool>("Use Picard","true");
          gotValue = false;
        } else {
          // ERROR ;
        }
        if (defPL.isParameter("reduction_factor")) {
          haveSSF = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("reduction_factor")) {
                value = ecsPL.sublist(it->first).get<std::string>("reduction_factor");
                haveSSF = true;
	      }
    	    }
          }
        }
        if (defPL.isParameter("increase_factor")) {
          haveSSF = true;
        } else {
          for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	    if (it->first != "restart") {
              if (ecsPL.sublist(it->first).isParameter("increase_factor")) {
                haveSSF = true;
    	      }
    	    }
          }
        }
        list.sublist("Time Integration Mode").sublist("Steady") = steadyPL;
      } else {
        // proceed, user really meant Initialize to Steady case
        if (def_list->sublist("simulation").isParameter("verbosity")) {
          std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
          if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Creating Initialize to Steady Execution Control."<< std::endl;
          }
        }
        Teuchos::Array<double> start_times;
        Teuchos::Array<double> init_steps;
        Teuchos::Array<double> max_steps;
        Teuchos::ParameterList timesPL;
        Teuchos::ParameterList initPL;
        std::string value;
        bool gotValue = false;
        for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
	  if (it->first != "restart") {
	    std::string mode("none");
	    if (defPL.isParameter("mode")) mode = defPL.get<std::string>("mode");
            if (ecsPL.sublist(it->first).isParameter("mode")) mode = ecsPL.sublist(it->first).get<std::string>("mode");
	    if (strcmp(mode.c_str(),"steady")==0) {
	      if (ecsPL.sublist(it->first).isParameter("start")) {
                value = ecsPL.sublist(it->first).get<std::string>("start");
	        double time = get_time_value(value,*def_list);
	        initPL.set<double>("Start",time);
	      }
	      if (ecsPL.sublist(it->first).isParameter("end")) {
                value = ecsPL.sublist(it->first).get<std::string>("end");
	        double time = get_time_value(value,*def_list);
	        initPL.set<double>("Switch",time);
	      }
	      if (ecsPL.sublist(it->first).isParameter("init_dt")) {
                value = ecsPL.sublist(it->first).get<std::string>("init_dt");
	        initPL.set<double>("Steady Initial Time Step",get_time_value(value,*def_list));
	      } 
	      if (ecsPL.sublist(it->first).isParameter("method")) {
                value = ecsPL.sublist(it->first).get<std::string>("method");
	        if (strcmp(value.c_str(),"true")==0) 
	          initPL.set<bool>("Use Picard",true);
	      } 
	      if (ecsPL.sublist(it->first).isParameter("reduction_factor")) {
                haveSSF = true;
	      } 
	      if (ecsPL.sublist(it->first).isParameter("increase_factor")) {
                haveSSF = true;
	      } 
	    } else {
	      if (ecsPL.sublist(it->first).isParameter("start")) {
                value = ecsPL.sublist(it->first).get<std::string>("start");
	        double time = get_time_value(value,*def_list);
	        start_times.append(time);
	      }
	      if (ecsPL.sublist(it->first).isParameter("end")) {
                value = ecsPL.sublist(it->first).get<std::string>("end");
	        double time = get_time_value(value,*def_list);
	        initPL.set<double>("End",time);
	      }
	      if (ecsPL.sublist(it->first).isParameter("init_dt")) {
                value = ecsPL.sublist(it->first).get<std::string>("init_dt");
	        gotValue = true;
	      } else {
	        if (defPL.isParameter("init_dt")) {
                  value = defPL.get<std::string>("init_dt");
	          gotValue = true;
	        }
	      }
	      if (gotValue) {
	        init_steps.append(get_time_value(value,*def_list));
	        gotValue = false;
	      }
	    }
	  }
        }
        initPL.set<double>("Transient Initial Time Step",init_steps[0]);
        list.sublist("Time Integration Mode").sublist("Initialize To Steady") = initPL;
	if (start_times.length() > 1) {
          timesPL.set<Teuchos::Array<double> >("Start Times",start_times);
          timesPL.set<Teuchos::Array<double> >("Initial Time Step",init_steps);
          list.sublist("Time Period Control") = timesPL;
	}
      }
    }
  }

  // get numerical controls node
  if (def_list->sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Numerical Controls."<< std::endl;
    }
  }
  Teuchos::ParameterList ncPL;
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("numerical_controls"));
  // grab some variables saved in the defPL and ecsPL
  Teuchos::ParameterList tcPL, ssPL;
  // first check the default list
  if ( defPL.isParameter("mode") ) {
    if (defPL.get<std::string>("mode") == "transient") {
      if (defPL.isParameter("increase_factor")) {
        tcPL.set<double>("transient time step increase factor",
                         get_double_constant(defPL.get<std::string>("increase_factor"),*def_list));
      }
      if (defPL.isParameter("reduction_factor")) {
        tcPL.set<double>("transient time step reduction factor",
                         get_double_constant(defPL.get<std::string>("reduction_factor"),*def_list));
      }
      if (defPL.isParameter("max_dt")) {
        tcPL.set<double>("transient max time step",
                         get_time_value(defPL.get<std::string>("max_dt"),*def_list));
      }
    }
    if (defPL.get<std::string>("mode") == "steady") {
      if (defPL.isParameter("increase_factor")) {
        ssPL.set<double>("steady time step increase factor",
                         get_double_constant(defPL.get<std::string>("increase_factor"),*def_list));
      }
      if (defPL.isParameter("reduction_factor")) {
        ssPL.set<double>("steady time step reduction factor",
                         get_double_constant(defPL.get<std::string>("reduction_factor"),*def_list));
      }
      if (defPL.isParameter("max_dt")) {
        ssPL.set<double>("steady max time step",
                         get_time_value(defPL.get<std::string>("max_dt"),*def_list));
      }
    }
  }
  // next check the individual execution controls, this will overwrite default values
  for (Teuchos::ParameterList::ConstIterator it = ecsPL.begin(); it != ecsPL.end(); ++it) {
    if (ecsPL.sublist(it->first).isParameter("mode")) {
      if (ecsPL.sublist(it->first).get<std::string>("mode") == "transient") {
        if (ecsPL.sublist(it->first).isParameter("increase_factor")) {
          tcPL.set<double>("transient time step increase factor",
                           get_double_constant(ecsPL.sublist(it->first).get<std::string>("increase_factor"),*def_list));
        }
        if (ecsPL.sublist(it->first).isParameter("reduction_factor")) {
          tcPL.set<double>("transient time step reduction factor",
                           get_double_constant(ecsPL.sublist(it->first).get<std::string>("reduction_factor"),*def_list));
        }
        if (ecsPL.sublist(it->first).isParameter("max_dt")) {
          tcPL.set<double>("transient max time step",
                           get_time_value(ecsPL.sublist(it->first).get<std::string>("max_dt"),*def_list));
        }
      }
      if (ecsPL.sublist(it->first).get<std::string>("mode") == "steady") {
        if (ecsPL.sublist(it->first).isParameter("increase_factor")) {
          ssPL.set<double>("steady time step increase factor",
                           get_double_constant(ecsPL.sublist(it->first).get<std::string>("increase_factor"),*def_list));
        }
        if (ecsPL.sublist(it->first).isParameter("reduction_factor")) {
          ssPL.set<double>("steady time step reduction factor",
                           get_double_constant(ecsPL.sublist(it->first).get<std::string>("reduction_factor"),*def_list));
        }
        if (ecsPL.sublist(it->first).isParameter("max_dt")) {
          ssPL.set<double>("steady max time step",
                           get_time_value(ecsPL.sublist(it->first).get<std::string>("max_dt"),*def_list));
        }
      }
    }
  }
  
  for (int i=0; i<nodeList->getLength(); i++) {
    DOMNode* ncNode = nodeList->item(i);
    if (DOMNode::ELEMENT_NODE == ncNode->getNodeType()) {
      DOMNodeList* childList = ncNode->getChildNodes();
      for(int j=0; j<childList->getLength(); j++) {
        DOMNode* tmpNode = childList->item(j) ;
        if (DOMNode::ELEMENT_NODE == tmpNode->getNodeType()) {
          char* nodeName = XMLString::transcode(tmpNode->getNodeName());
          if (strcmp(nodeName,"steady-state_controls")==0) {
            // loop through children and deal with them
            DOMNodeList* children = tmpNode->getChildNodes();
            for (int k=0; k<children->getLength(); k++) {
              DOMNode* currentNode = children->item(k) ;
              if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	        char* tagname = XMLString::transcode(currentNode->getNodeName());
                if (strcmp(tagname,"min_iterations")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<int>("steady min iterations",get_int_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"max_iterations")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<int>("steady max iterations",get_int_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"limit_iterations")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<int>("steady limit iterations",get_int_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"max_preconditioner_lag_iterations")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<int>("steady max preconditioner lag iterations",get_int_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"nonlinear_tolerance")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<double>("steady nonlinear tolerance",get_double_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"error_rel_tol")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<double>("steady error rel tol",get_double_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"error_abs_tol")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    ssPL.set<double>("steady error abs tol",get_double_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"restart_tolerance_factor")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent()); 
                    ssPL.set<double>("steady restart tolerance relaxation factor",get_double_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"restart_tolerance_relaxation_factor")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent()); 
                    ssPL.set<double>("steady restart tolerance relaxation factor damping",get_double_constant(textContent,*def_list));
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"initialize_with_darcy")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
		    bool iwd(false);
		    std::string(textContent) == "true" ? iwd = true : iwd = false;  
		    if (!iwd)
		        std::string(textContent) == "1" ? iwd = true : iwd = false;
		    ssPL.set<bool>("steady initialize with darcy",iwd);
                    XMLString::release(&textContent);
                } else if (strcmp(tagname,"pseudo_time_integrator")==0) {
                    Teuchos::ParameterList ptiPL;
                    DOMNodeList* kids = currentNode->getChildNodes();
                    for (int l=0; l<kids->getLength(); l++) {
                        DOMNode* curNode = kids->item(l) ;
                        if (DOMNode::ELEMENT_NODE == curNode->getNodeType()) {
                            char* tag = XMLString::transcode(curNode->getNodeName());
                            if (strcmp(tag,"method")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
				if (strcmp(textContent,"picard")==0) {
                                  ptiPL.set<std::string>("pseudo time integrator time integration method","Picard");
				}
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"preconditioner")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
				if (strcmp(textContent,"trilinos_ml")==0) {
                                  ptiPL.set<std::string>("pseudo time integrator preconditioner","Trilinos ML");
				} else if (strcmp(textContent,"hypre_amg")==0) {
                                  ptiPL.set<std::string>("pseudo time integrator preconditioner","Hypre AMG");
				} else if (strcmp(textContent,"block_ilu")==0) {
                                  ptiPL.set<std::string>("pseudo time integrator preconditioner","Block ILU");
				}
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"linear_solver")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
				if (strcmp(textContent,"aztec00")==0) {
                                  ptiPL.set<std::string>("pseudo time integrator linear solver","AztecOO");
				}
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"control_options")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
                                ptiPL.set<std::string>("pseudo time integrator error control options",textContent);
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"max_iterations")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
                                ptiPL.set<int>("pseudo time integrator picard maximum number of iterations",
						   get_int_constant(textContent,*def_list));
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"clipping_saturation")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
                                ptiPL.set<double>("pseudo time integrator clipping saturation value",
						  get_double_constant(textContent,*def_list));
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"convergence_tolerance")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
                                ptiPL.set<double>("pseudo time integrator picard convergence tolerance",
						  get_double_constant(textContent,*def_list));
                                XMLString::release(&textContent);
                            } else if (strcmp(tag,"initialize_with_darcy")==0) {
                                textContent = XMLString::transcode(curNode->getTextContent());
                                bool iwd(false);
		                std::string(textContent) == "true" ? iwd = true : iwd = false;  
		                if (!iwd)
		                    std::string(textContent) == "1" ? iwd = true : iwd = false;  
                                ptiPL.set<bool>("pseudo time integrator initialize with darcy",iwd);
                                XMLString::release(&textContent);
                            }
                        }
                    }
                    ssPL.sublist("Steady-State Psuedo-Time Implicit Solver") = ptiPL;
                }
              }
	    }
	    // get items from steadyPL, from execution_controls
	    if (ecsPL.isSublist("steady")) {
	      if (ecsPL.sublist("steady").isParameter("max_dt")) {
		ssPL.set<double>("steady max time step",get_time_value(ecsPL.sublist("steady").get<std::string>("max_dt"),*def_list));
	      }
	    }

            //list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Steady-State Implicit Time Integration") = ssPL;
	  }
          else if (strcmp(nodeName,"transient_controls")==0) {
	    // check for incr/red factors from execution_controls first
	    // grab integration method, then loop through it's attributes
            DOMElement* tcElement = static_cast<DOMElement*>(tmpNode);
            DOMNodeList* tmpList = tcElement->getElementsByTagName(XMLString::transcode("bdf1_integration_method"));
	    if (tmpList->getLength() > 0) {
	      DOMElement* bdfElement = static_cast<DOMElement*>(tmpList->item(0));
	      if (bdfElement->hasAttribute(XMLString::transcode("min_iterations"))){
		textContent = XMLString::transcode(
			      bdfElement->getAttribute(XMLString::transcode("min_iterations")));
                tcPL.set<int>("transient min iterations",get_int_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("max_iterations"))){
		textContent = XMLString::transcode(
		   	      bdfElement->getAttribute(XMLString::transcode("max_iterations")));
                tcPL.set<int>("transient max iterations",get_int_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("limit_iterations"))){
		textContent = XMLString::transcode(
			      bdfElement->getAttribute(XMLString::transcode("limit_iterations")));
                tcPL.set<int>("transient limit iterations",get_int_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("nonlinear_tolerance"))){
		textContent = XMLString::transcode(
			      bdfElement->getAttribute(XMLString::transcode("nonlinear_tolerance")));
                tcPL.set<double>("transient nonlinear tolerance",get_double_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("max_divergent_iterations"))){
		textContent = XMLString::transcode(
			      bdfElement->getAttribute(XMLString::transcode("max_divergent_iterations")));
                tcPL.set<int>("transient max divergent iterations",get_int_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }  
	      if (bdfElement->hasAttribute(XMLString::transcode("max_preconditioner_lag_iterations"))){
		textContent = XMLString::transcode(
		              bdfElement->getAttribute(XMLString::transcode("max_preconditioner_lag_iterations")));
                tcPL.set<int>("transient max preconditioner lag iterations",get_int_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("nonlinear_iteration_damping_factor"))){
		textContent = XMLString::transcode(
		           bdfElement->getAttribute(XMLString::transcode("nonlinear_iteration_damping_factor")));
                tcPL.set<double>("transient nonlinear iteration damping factor",get_double_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("nonlinear_iteration_divergence_factor"))){
		textContent = XMLString::transcode(
		           bdfElement->getAttribute(XMLString::transcode("nonlinear_iteration_divergence_factor")));
                tcPL.set<double>("transient nonlinear iteration divergence factor",get_double_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("restart_tolerance_factor"))){
		textContent = XMLString::transcode(
		            bdfElement->getAttribute(XMLString::transcode("restart_tolerance_factor")));
                tcPL.set<double>("transient restart tolerance relaxation factor",get_double_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("restart_tolerance_relaxation_factor"))){
		textContent = XMLString::transcode(
		            bdfElement->getAttribute(XMLString::transcode("restart_tolerance_relaxation_factor")));
                tcPL.set<double>("transient restart tolerance relaxation factor damping",get_double_constant(textContent,*def_list));
                XMLString::release(&textContent);
	      }
	      if (bdfElement->hasAttribute(XMLString::transcode("initialize_with_darcy"))) {
                textContent = XMLString::transcode(bdfElement->getAttribute(XMLString::transcode("initialize_with_darcy")));		                     bool iwd(false);
		std::string(textContent) == "true" ? iwd = true : iwd = false;  
		if (!iwd)
		    std::string(textContent) == "1" ? iwd = true : iwd = false;  
		tcPL.set<bool>("transient initialize with darcy",iwd);
                XMLString::release(&textContent);
	      }

	    }

	    // grab preconditioner node and loop through it's childern to get information
            tmpList = tcElement->getElementsByTagName(XMLString::transcode("preconditioner"));
	    if (tmpList->getLength() > 0) {
	      DOMNode* preconNode = tmpList->item(0);
	      nodeAttr = preconNode->getAttributes()->getNamedItem(XMLString::transcode("name"));
	      if (nodeAttr) {
                textContent = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing preconditioner - \n" ;
	        msg << "No attribute name found for preconditioner. Options are: trilinos_ml, hypre_amg, block_ilu\n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      if (strcmp(textContent,"trilinos_ml")==0) {
                tcPL.set<std::string>("transient preconditioner","Trilinos ML");
                DOMNodeList* children = preconNode->getChildNodes();
	        Teuchos::ParameterList preconPL;
                for (int k=0; k<children->getLength(); k++) {
                  DOMNode* currentNode = children->item(k) ;
                  if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	            char* tagname = XMLString::transcode(currentNode->getNodeName());
                    if (strcmp(tagname,"trilinos_smoother_type")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
		      if (strcmp(textContent2,"jacobi")==0) {
                        preconPL.set<std::string>("ML smoother type","Jacobi");
		      }else if (strcmp(textContent2,"gauss_seidel")==0) {
                        preconPL.set<std::string>("ML smoother type","Gauss-Seidel");
		      }else if (strcmp(textContent2,"ilu")==0) {
                        preconPL.set<std::string>("ML smoother type","ILU");
		     }
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"trilinos_threshold")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("ML aggregation threshold",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"trilinos_smoother_sweeps")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("ML smoother sweeps",get_int_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"trilinos_cycle_applications")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("ML cycle applications",get_int_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    }
		  }
	        }
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Preconditioners").sublist("Trilinos ML") = preconPL;
              } else if (strcmp(textContent,"hypre_amg")==0) {
                tcPL.set<std::string>("transient preconditioner","Hypre AMG");
                DOMNodeList* children = preconNode->getChildNodes();
	        Teuchos::ParameterList preconPL;
                for (int k=0; k<children->getLength(); k++) {
                  DOMNode* currentNode = children->item(k) ;
                  if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	            char* tagname = XMLString::transcode(currentNode->getNodeName());
                    if (strcmp(tagname,"hypre_cycle_applications")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("Hypre AMG cycle applications",get_int_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"hypre_smoother_sweeps")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("Hypre AMG smoother sweeps",get_int_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"hypre_tolerance")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("Hypre AMG tolerance",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    } else if (strcmp(tagname,"hypre_strong_threshold")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("Hypre AMG strong threshold",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		    }
		  }
	        }
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Preconditioners").sublist("Hypre AMG") = preconPL;
	      } else if (strcmp(textContent,"block_ilu")==0) {
                tcPL.set<std::string>("transient preconditioner","Block ILU");
                DOMNodeList* children = preconNode->getChildNodes();
	            Teuchos::ParameterList preconPL;
                for (int k=0; k<children->getLength(); k++) {
                  DOMNode* currentNode = children->item(k) ;
                  if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	            char* tagname = XMLString::transcode(currentNode->getNodeName());
                    if (strcmp(tagname,"ilu_overlap")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("Block ILU overlap",get_int_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		        } else if (strcmp(tagname,"ilu_relax")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("Block ILU relax value",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		        } else if (strcmp(tagname,"ilu_rel_threshold")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("Block ILU relative threshold",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		        } else if (strcmp(tagname,"ilu_abs_threshold")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<double>("Block ILU absolute threshold",get_double_constant(textContent2,*def_list));
                      XMLString::release(&textContent2);
		        } else if (strcmp(tagname,"ilu_level_of_fill")==0) {
                      char* textContent2 = XMLString::transcode(currentNode->getTextContent());
                      preconPL.set<int>("Block ILU level of fill",get_int_constant(textContent2,*def_list));
                    XMLString::release(&textContent2);
		        }
		      }
	        }
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Preconditioners").sublist("Block ILU") = preconPL;
 
	      }
              XMLString::release(&textContent);
	    
	      }
            //list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Transient Implicit Time Integration") = tcPL;
          }
          else if (strcmp(nodeName,"nonlinear_solver")==0) {
	      Teuchos::ParameterList nlPL;
	      attrMap = tmpNode->getAttributes();
	      nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	      if (nodeAttr) {
                textContent = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing numerical_controls - " ;
	        msg << "No attribute name found for nonlinear_solver. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }

	      if (strcmp(textContent,"nka")==0) {
		nlPL.set<std::string>("Nonlinear Solver Type","NKA");
	      } else if (strcmp(textContent,"newton")==0) {
		nlPL.set<std::string>("Nonlinear Solver Type","Newton");
	      } else if (strcmp(textContent,"inexact newton")==0) {
		nlPL.set<std::string>("Nonlinear Solver Type","inexact Newton");
	      }
	      XMLString::release(&textContent);
	      list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Nonlinear Solver") = nlPL;
	    }
          else if (strcmp(nodeName,"linear_solver")==0) {
              Teuchos::ParameterList lsPL;
              Teuchos::ParameterList pcPL;
              bool usePCPL=false;
              // loop through children and deal with them
              DOMNodeList* children = tmpNode->getChildNodes();
              for (int k=0; k<children->getLength(); k++) {
                DOMNode* currentNode = children->item(k) ;
                if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
                  char* tagname = XMLString::transcode(currentNode->getNodeName());
                  if (strcmp(tagname,"method")==0) {
                        textContent = XMLString::transcode(currentNode->getTextContent());
                        lsPL.set<std::string>("linear solver preconditioner",textContent);
                        XMLString::release(&textContent);
                  } else if (strcmp(tagname,"max_iterations")==0) {
                        textContent = XMLString::transcode(currentNode->getTextContent());
                        lsPL.set<int>("linear solver maximum iterations",get_int_constant(textContent,*def_list));
                        XMLString::release(&textContent);
                  } else if (strcmp(tagname,"tolerance")==0) {
                        textContent = XMLString::transcode(currentNode->getTextContent());
                        lsPL.set<double>("linear solver tolerance",get_double_constant(textContent,*def_list));
                        XMLString::release(&textContent);
                  } else if (strcmp(tagname,"cfl")==0) {
                        textContent = XMLString::transcode(currentNode->getTextContent());
                        tpkPL.set<double>("CFL",get_double_constant(textContent,*def_list));
                        XMLString::release(&textContent);
                  } else if (strcmp(tagname,"preconditioner")==0) {
                      // which precondition is stored in attribute, options are: trilinos_ml, hypre_amg, block_ilu
                      attrMap = currentNode->getAttributes();
                      nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
		      if (nodeAttr) {
                        textContent = XMLString::transcode(nodeAttr->getNodeValue());
	              } else {
	                Errors::Message msg;
	                msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing linear_solver - " ;
	                msg << "No attribute name found for preconditioner. \n" ;
	                msg << "  Please correct and try again \n" ;
	                Exceptions::amanzi_throw(msg);
	              }
                      usePCPL = true;
                      if (strcmp(textContent,"hypre_amg")==0) {
			// TODO:: EIB - this is hacky, really need to check if list exist, if it doesn't need to flag 
			//              so this doesn't get overwritten later when the list actually gets created.
		        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Steady-State Implicit Time Integration").set<std::string>("steady preconditioner","Hypre AMG");
                        // loop through children and deal with them
                        DOMNodeList* preconChildren= currentNode->getChildNodes();
                        for (int l=0; l<preconChildren->getLength();l++) {
                          DOMNode* currentKid = preconChildren->item(l) ;
                          if (DOMNode::ELEMENT_NODE == currentKid->getNodeType()) {
    	                    char* kidname = XMLString::transcode(currentKid->getNodeName());
                            if (strcmp(kidname,"hypre_cycle_applications")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Hypre AMG").set<int>("Hypre AMG cycle applications",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"hypre_smoother_sweeps")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Hypre AMG").set<int>("Hypre AMG smoother sweeps",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"hypre_tolerance")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Hypre AMG").set<double>("Hypre AMG tolerance",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"hypre_strong_threshold")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Hypre AMG").set<double>("Hypre AMG strong threshold",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    }
			  }
			}
		    }
                      else if (strcmp(textContent,"trilinos_ml")==0) {
			// TODO:: EIB - this is hacky, really need to check if list exist, if it doesn't need to flag 
			//              so this doesn't get overwritten later when the list actually gets created.
		        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Steady-State Implicit Time Integration").set<std::string>("steady preconditioner","Trilinos ML");
                        // loop through children and deal with them
                        DOMNodeList* preconChildren= currentNode->getChildNodes();
                        for (int l=0; l<preconChildren->getLength();l++) {
                          DOMNode* currentKid = preconChildren->item(l) ;
                          if (DOMNode::ELEMENT_NODE == currentKid->getNodeType()) {
    	                    char* kidname = XMLString::transcode(currentKid->getNodeName());
                            if (strcmp(kidname,"trilinos_smoother_type")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
		                if (strcmp(elemContent,"jacobi")==0) {
                                  pcPL.sublist("Trilinos ML").set<std::string>("ML smoother type","Jacobi");
		                }else if (strcmp(elemContent,"gauss_seidel")==0) {
                                  pcPL.sublist("Trilinos ML").set<std::string>("ML smoother type","Gauss-Seidel");
		                }else if (strcmp(elemContent,"ilu")==0) {
                                  pcPL.sublist("Trilinos ML").set<std::string>("ML smoother type","ILU");
		                }
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"trilinos_threshold")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Trilinos ML").set<double>("ML aggregation threshold",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"trilinos_smoother_sweeps")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Trilinos ML").set<int>("ML smoother sweeps",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"trilinos_cycle_applications")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Trilinos ML").set<int>("ML cycle applications",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    }
			  }
			}
		    }
                      else if (strcmp(textContent,"block_ilu")==0) {
			// TODO:: EIB - this is hacky, really need to check if list exist, if it doesn't need to flag 
			//              so this doesn't get overwritten later when the list actually gets created.
		        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Steady-State Implicit Time Integration").set<std::string>("steady preconditioner","Block ILU");
                        // loop through children and deal with them
                        DOMNodeList* preconChildren= currentNode->getChildNodes();
                        for (int l=0; l<preconChildren->getLength();l++) {
                          DOMNode* currentKid = preconChildren->item(l) ;
                          if (DOMNode::ELEMENT_NODE == currentKid->getNodeType()) {
    	                    char* kidname = XMLString::transcode(currentKid->getNodeName());
                            if (strcmp(kidname,"ilu_overlap")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Block ILU").set<int>("Block ILU overlap",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"ilu_relax")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Block ILU").set<double>("Block ILU relax value",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"ilu_rel_threshold")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Block ILU").set<double>("Block ILU relative threshold",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"ilu_abs_threshold")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Block ILU").set<double>("Block ILU absolute threshold",get_double_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    } else if (strcmp(kidname,"ilu_level_of_fill")==0) {
                                elemContent = XMLString::transcode(currentKid->getTextContent());
                                pcPL.sublist("Block ILU").set<int>("Block ILU level of fill",get_int_constant(elemContent,*def_list));
                                XMLString::release(&elemContent);
			    }
			  }
			}
		    }
                      XMLString::release(&textContent);
                  }
                }
              }
              list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Linear Solver") = lsPL;
	      if (usePCPL)
                list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Preconditioners") = pcPL;
	      if (flowON)
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Flow Process Kernel") = fpkPL;
	      if (transportON)
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Transport Process Kernel") = tpkPL;
	      if (chemistryON)
	        list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Chemistry Process Kernel") = cpkPL;
            }
          else if (strcmp(nodeName,"nonlinear_solver")==0) {
              // EIB: creating sub for section that doesn't actually exist yet in the New Schema, but does in the Input Spec
              Teuchos::ParameterList nlsPL;
              DOMNodeList* children = tmpNode->getChildNodes();
              for (int k=0; k<children->getLength(); k++) {
                DOMNode* currentNode = children->item(k) ;
                if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	          char* tagname = XMLString::transcode(currentNode->getNodeName());
                  if (strcmp(tagname,"nonlinear_solver_type")==0) {
                    textContent = XMLString::transcode(currentNode->getTextContent());
                    nlsPL.set<std::string>("Nonlinear Solver Type",textContent);
                    XMLString::release(&textContent);
		  }
                }
	      }
              list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Nonlinear Solver") = nlsPL;
            }
          else if (strcmp(nodeName,"chemistry_controls")==0) {
	      Teuchos::ParameterList chemistryPL;
	      // go ahead and add bdg file to PL
	      // build bgd filename
	      std::string bgdfilename;
	      if (def_list->isParameter("xmlfilename") ) {
                bgdfilename = def_list->get<std::string>("xmlfilename") ;
                std::string new_extension(".bgd");
                size_t pos = bgdfilename.find(".xml");
                bgdfilename.replace(pos, (size_t)4, new_extension, (size_t)0, (size_t)4);
	      } else {
		// defaulting to hardcoded name
	        bgdfilename = "isotherms.bgd" ;
	      }
	      // add bgd file and parameters to list
	      Teuchos::ParameterList bgdPL;
	      bgdPL.set<std::string>("Format","simple");
	      bgdPL.set<std::string>("File",bgdfilename);
	      chemistryPL.sublist("Thermodynamic Database") = bgdPL;
	      chemistryPL.set<std::string>("Activity Model","unit");
	      Teuchos::Array<std::string> verb;
              if (def_list->sublist("simulation").isParameter("verbosity")) {
                std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
                if (verbosity == "extreme") {
		    verb.append("error");
	            chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
		} else if (verbosity == "high") {
		    verb.append("warning");
	            chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
		} else if (verbosity == "medium") {
		    verb.append("verbose");
	            chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
		} else if (verbosity == "low") {
		    verb.append("terse");
	            chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
		} else {
		    verb.append("silent");
	            chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
		}
	      } else {
		verb.append("silent");
	        chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
	      }
	      // loop over chemistry controls to get other options to add to PL
              DOMNodeList* children = tmpNode->getChildNodes();
              for (int k=0; k<children->getLength(); k++) {
                DOMNode* currentNode = children->item(k) ;
                if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
    	          char* tagname = XMLString::transcode(currentNode->getNodeName());
                  if (strcmp(tagname,"chem_tolerance")==0) {
		    if (currentNode) {
                      textContent = XMLString::transcode(currentNode->getTextContent());
                      chemistryPL.set<double>("Tolerance",get_double_constant(textContent,*def_list));
                      XMLString::release(&textContent);
		    } else {
		      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing numerical_controls->chemistry_controls - " ;
		      msg << "chem_tolerance was missing or ill-formed. \n  Please correct and try again \n" ;
		      Exceptions::amanzi_throw(msg);
		    }
		  } else if (strcmp(tagname,"chem_max_newton_iterations")==0) {
		    if (currentNode) {
                      textContent = XMLString::transcode(currentNode->getTextContent());
                      chemistryPL.set<int>("Maximum Newton Iterations",get_int_constant(textContent,*def_list));
                      XMLString::release(&textContent);
		    } else {
		      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing numerical_controls->chemistry_controls - " ;
		      msg << "chem_max_newton_iterations was missing or ill-formed. \n  Please correct and try again \n" ;
		      Exceptions::amanzi_throw(msg);
		    }
		  // TODO:: EIB - this need to be added to schema!!
		  } else if (strcmp(tagname,"chem_max_time_step")==0) {
		    if (currentNode) {
                      textContent = XMLString::transcode(currentNode->getTextContent());
                      chemistryPL.set<double>("Max Time Step (s)",get_double_constant(textContent,*def_list));
                      XMLString::release(&textContent);
		    } else {
		      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing numerical_controls->chemistry_controls - " ;
		      msg << "chem_max_time_step was missing or ill-formed. \n  Please correct and try again \n" ;
		      Exceptions::amanzi_throw(msg);
		    }
		  } else {
		     // TODO:: EIB - should I error or just ignore???
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing numerical_controls->chemistry_controls - " ;
		     msg << tagname << " was unrecognized option. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
                }
	      }
	      def_list->sublist("Chemistry") = chemistryPL;
	    }
        }
      }
    }      
  }
  list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Steady-State Implicit Time Integration") = ssPL;
  list.sublist("Numerical Control Parameters").sublist(meshbase).sublist("Transient Implicit Time Integration") = tcPL;
  
  // TODO: EIB - got back and get the transport algorithm and chemisty model names
  
  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_phases(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNodeList* nodeList2;
  DOMNode* nodeTmp;
  DOMNode* nodeTmp2;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* textContent;
  char* textContent2;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Phases."<< std::endl;
    }
  }

  // get phases node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("phases"));
  DOMNode* nodeEC = nodeList->item(0);
  DOMElement* elementEC = static_cast<DOMElement*>(nodeEC);

  // get comments node - EIB: removed set, old spec can't handle comments at this level
  nodeList = elementEC->getElementsByTagName(XMLString::transcode("comments"));
  if (nodeList->getLength() > 0) {
    nodeTmp = nodeList->item(0);
    textContent = XMLString::transcode(nodeTmp->getTextContent());
    //list.set<std::string>("comments",textContent);
    XMLString::release(&textContent);
  }

  // get liquid_phase node
  nodeList = elementEC->getElementsByTagName(XMLString::transcode("liquid_phase"));
  if (nodeList->getLength() > 0) {
    nodeTmp = nodeList->item(0);
    attrMap = nodeTmp->getAttributes();
    nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
    char* phaseName = XMLString::transcode(nodeAttr->getNodeValue());
    if (nodeAttr) {
      phaseName = XMLString::transcode(nodeAttr->getNodeValue());
    } else {
      Errors::Message msg;
      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing phases - " ;
      msg << "No attribute name found for liquid_phase. \n" ;
      msg << "  Please correct and try again \n" ;
      Exceptions::amanzi_throw(msg);
    }

    DOMNodeList* childern = nodeTmp->getChildNodes();
    for (int i=0; i<childern->getLength(); i++) {
      DOMNode* cur = childern->item(i) ;
      if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
        tagName  = XMLString::transcode(cur->getNodeName());
        textContent = XMLString::transcode(cur->getTextContent());
	//TODO: NOTE: EIB - skipping EOS, not currently supported
	if (strcmp(tagName,"viscosity")==0){
          list.sublist("Aqueous").sublist("Phase Properties").sublist("Viscosity: Uniform").set<double>("Viscosity",get_double_constant(textContent,def_list));
	}
	else if (strcmp(tagName,"density")==0) {
          list.sublist("Aqueous").sublist("Phase Properties").sublist("Density: Uniform").set<double>("Density",get_double_constant(textContent,def_list));
	}
	else if (strcmp(tagName,"dissolved_components")==0) {
	  Teuchos::ParameterList dcPL;
	  Teuchos::Array<double> diffusion;
	  Teuchos::Array<std::string> solutes;
          DOMElement* discompElem = static_cast<DOMElement*>(cur);
          nodeList2 = discompElem->getElementsByTagName(XMLString::transcode("solutes"));
          if (nodeList2->getLength() > 0) {
            nodeTmp2 = nodeList2->item(0);
            DOMNodeList* kids = nodeTmp2->getChildNodes();
            for (int j=0; j<kids->getLength(); j++) {
              DOMNode* curKid = kids->item(j) ;
              if (DOMNode::ELEMENT_NODE == curKid->getNodeType()) {
                tagName  = XMLString::transcode(curKid->getNodeName());
	        if (strcmp(tagName,"solute")==0){
		  // put value in solutes array
                  textContent2 = XMLString::transcode(curKid->getTextContent());
		  solutes.append(textContent2);
                  XMLString::release(&textContent2);
		  // put attribute - coefficient_of_diffusion in diffusion array
	          attrMap = curKid->getAttributes();
                  nodeAttr = attrMap->getNamedItem(XMLString::transcode("coefficient_of_diffusion"));
		  if (nodeAttr) {
                    textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	          } else {
	            Errors::Message msg;
	            msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing phases - " ;
	            msg << "No attribute coefficient_of_diffusion found for solute. \n" ;
	            msg << "  Please correct and try again \n" ;
	            Exceptions::amanzi_throw(msg);
	          }

		  diffusion.append(get_double_constant(textContent2,def_list));
                  XMLString::release(&textContent2);
		}
	      }
	    }
	  }
	  dcPL.set<Teuchos::Array<std::string> >("Component Solutes",solutes);
	  list.sublist("Aqueous").sublist("Phase Components").sublist(phaseName) = dcPL;
	}
        XMLString::release(&textContent);
      }
    }
  }

  // get any solid_phase node
  nodeList = elementEC->getElementsByTagName(XMLString::transcode("solid_phase"));
  if (nodeList->getLength() > 0) {
    Teuchos::ParameterList spPL;
    Teuchos::Array<std::string> minerals;
    nodeTmp = nodeList->item(0);
    DOMElement* solidElem = static_cast<DOMElement*>(nodeTmp);
    nodeList2 = solidElem->getElementsByTagName(XMLString::transcode("minerals"));
    if (nodeList2->getLength() > 0) {
      nodeTmp2 = nodeList2->item(0);
      DOMNodeList* kids = nodeTmp2->getChildNodes();
      for (int i=0; i<kids->getLength(); i++) {
        DOMNode* curKid = kids->item(i) ;
        if (DOMNode::ELEMENT_NODE == curKid->getNodeType()) {
          tagName  = XMLString::transcode(curKid->getNodeName());
	  if (strcmp(tagName,"mineral")==0){
            textContent2 = XMLString::transcode(curKid->getTextContent());
	    minerals.append(textContent2);
            XMLString::release(&textContent2);
	  }
	}
      }
    }
    spPL.set<Teuchos::Array<std::string> >("Minerals",minerals);
    list.sublist("Solid") = spPL;
  }

  return list;
  
}


/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_regions(DOMDocument* xmlDoc, Teuchos::ParameterList* def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNode* nodeTmp;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* nodeName;
  char* textContent;
  char* textContent2;
  char* char_array;

  if (def_list->sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list->sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Regions."<< std::endl;
    }
  }
  Teuchos::ParameterList reg_names;

  // get regions node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("regions"));
  DOMNode* nodeRgn = nodeList->item(0);
  DOMElement* elementRgn = static_cast<DOMElement*>(nodeRgn);

  // just loop over the children and deal with them as they come
  // new options: comment, region, box, point
  DOMNodeList* childern = nodeRgn->getChildNodes();
  for (int i=0; i<childern->getLength(); i++) {
    DOMNode* cur = childern->item(i) ;
    if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
      tagName  = XMLString::transcode(cur->getNodeName());
      /* NOTE: EIB - geometry doesn't deal with extra comment node
      if (strcmp(tagName,"comments") == 0){
        textContent = XMLString::transcode(cur->getTextContent());
        list.set<std::string>("comments",textContent);
        XMLString::release(&textContent);
      } */
      if  (strcmp(tagName,"region") == 0){
	attrMap = cur->getAttributes();
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute name found for region. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	// add region name to array of region names
	if (reg_names.isParameter(textContent)) {
		// warn, region of this name already exists, overwriting
	} else {
	  reg_names.set<std::string>(textContent,"region");
	}
	// deal with children: comments, box/file
        DOMNodeList* kids = cur->getChildNodes();
        for (int j=0; j<kids->getLength(); j++) {
          DOMNode* curKid = kids->item(j) ;
          if (DOMNode::ELEMENT_NODE == curKid->getNodeType()) {
            nodeName  = XMLString::transcode(curKid->getNodeName());
	    /*             
	    if (strcmp(tagName,"comments") == 0){
              textContent = XMLString::transcode(curKid->getTextContent());
              list.set<std::string>("comments",textContent);
              XMLString::release(&textContent);
            }
	    */
            if  (strcmp(nodeName,"box") == 0){
	      attrMap = curKid->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("low_coordinates"));
	      if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute low_coordinates found for box. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }

	      Teuchos::Array<double> low = make_coordinates(textContent2, *def_list);
              list.sublist(textContent).sublist("Region: Box").set<Teuchos::Array<double> >("Low Coordinate",low);
	      XMLString::release(&textContent2);
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("high_coordinates"));
              if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute high_coordinates found for box. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      Teuchos::Array<double> high = make_coordinates(textContent2,* def_list);
              list.sublist(textContent).sublist("Region: Box").set<Teuchos::Array<double> >("High Coordinate",high);
	      XMLString::release(&textContent2);
	    }
            else if  (strcmp(nodeName,"plane") == 0){
	      attrMap = curKid->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("location"));
              if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute location found for plane. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      Teuchos::Array<double> loc = make_coordinates(textContent2, *def_list);
              list.sublist(textContent).sublist("Region: Plane").set<Teuchos::Array<double> >("Location",loc);
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("normal"));
              if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute normal found for plane. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      Teuchos::Array<double> dir = make_coordinates(textContent2, *def_list);
              list.sublist(textContent).sublist("Region: Plane").set<Teuchos::Array<double> >("Direction",dir);
	      XMLString::release(&textContent2);
	    } else if  (strcmp(nodeName,"region_file") == 0){
	      //TODO: EIB - add file
	      Teuchos::ParameterList rfPL;
	      attrMap = curKid->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
              if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute name found for region_file. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      rfPL.set<std::string>("File",textContent2);
	      XMLString::release(&textContent2);
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("type"));
              if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	        msg << "No attribute type found for region_file. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      if  (strcmp(textContent2,"color") == 0){
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("label"));
                char* value;
		if (nodeAttr) {
                  value = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	          msg << "No attribute label found for color. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }
	        rfPL.set<int>("Value",atoi(value));
	        XMLString::release(&value);
                list.sublist(textContent).sublist("Region: Color Function") = rfPL;
	      }else if  (strcmp(textContent2,"labeled set") == 0){
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("label"));
                char* value ;
		if (nodeAttr) {
                  value = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	          msg << "No attribute label found for labeled set. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }
	        rfPL.set<std::string>("Label",value);
	        XMLString::release(&value);
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("format"));
		if (nodeAttr) {
                  value = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	          msg << "No attribute format found for labeled set. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }
	        if  (strcmp(value,"exodus ii") == 0){
	          rfPL.set<std::string>("Format","Exodus II");
		}
	        XMLString::release(&value);
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("entity"));
		if (nodeAttr) {
                  value = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	          msg << "No attribute entity found for labeled set. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }
	        rfPL.set<std::string>("Entity",value);
	        XMLString::release(&value);
                list.sublist(textContent).sublist("Region: Labeled Set") = rfPL;
	      }
	      XMLString::release(&textContent2);
	    }
	    XMLString::release(&nodeName);
	  }
	}
      }
      else if  (strcmp(tagName,"box") == 0){
	attrMap = cur->getAttributes();
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions- " ;
	  msg << "No attribute name found for box. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}
	// add region name to array of region names
	if (reg_names.isParameter(textContent)) {
		// warn, region of this name already exists, overwriting
	} else {
	  reg_names.set<std::string>(textContent,"box");
	}
	// get low coord
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("low_coordinates"));
	if (nodeAttr) {
          textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute low_coordinates found for box. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}
	Teuchos::Array<double> low = make_coordinates(textContent2, *def_list);
        list.sublist(textContent).sublist("Region: Box").set<Teuchos::Array<double> >("Low Coordinate",low);
	XMLString::release(&textContent2);
	// get high coord
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("high_coordinates"));
	if (nodeAttr) {
          textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute high_coordinates found for box. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}
	Teuchos::Array<double> high = make_coordinates(textContent2, *def_list);
        list.sublist(textContent).sublist("Region: Box").set<Teuchos::Array<double> >("High Coordinate",high);
	XMLString::release(&textContent2);
	XMLString::release(&textContent);
      }
      else if  (strcmp(tagName,"point") == 0){
	attrMap = cur->getAttributes();
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute name found for point. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	// add region name to array of region names
	if (reg_names.isParameter(textContent)) {
		// warn, region of this name already exists, overwriting
	} else {
	  reg_names.set<std::string>(textContent,"point");
	}
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("coordinate"));
	if (nodeAttr) {
          textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute coordinate found for point. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	Teuchos::Array<double> coord = make_coordinates(textContent2, *def_list);
        list.sublist(textContent).sublist("Region: Point").set<Teuchos::Array<double> >("Coordinate",coord);
	XMLString::release(&textContent);
	XMLString::release(&textContent2);
      } else if  (strcmp(tagName,"plane") == 0){
	attrMap = cur->getAttributes();
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	if (nodeAttr) {
          textContent = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute name found for plane. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	// add region name to array of region names
	if (reg_names.isParameter(textContent)) {
		// warn, region of this name already exists, overwriting
	} else {
	  reg_names.set<std::string>(textContent,"plane");
	}
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("location"));
	if (nodeAttr) {
          textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute location found for plane. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	Teuchos::Array<double> loc = make_coordinates(textContent2, *def_list);
        list.sublist(textContent).sublist("Region: Plane").set<Teuchos::Array<double> >("Location",loc);
        nodeAttr = attrMap->getNamedItem(XMLString::transcode("normal"));
	if (nodeAttr) {
          textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	} else {
	  Errors::Message msg;
	  msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing regions - " ;
	  msg << "No attribute normal found for plane. \n" ;
	  msg << "  Please correct and try again \n" ;
	  Exceptions::amanzi_throw(msg);
	}

	Teuchos::Array<double> dir = make_coordinates(textContent2, *def_list);
        list.sublist(textContent).sublist("Region: Plane").set<Teuchos::Array<double> >("Direction",dir);
	XMLString::release(&textContent);
	XMLString::release(&textContent2);
      }
      XMLString::release(&tagName);
    }
  }
  // add array of region names to def_list, use these names to check assigned_regions list against later
  def_list->sublist("regions") = reg_names; 


  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_materials(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNode* nodeTmp;
  DOMNode* nodeTmp2;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* propName;
  char* propValue;
  char* textContent;
  char* textContent2;
  char* char_array;
  char* attrName;
  char* attrValue;
  bool hasPerm = false;
  bool hasHC = false;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Materials."<< std::endl;
    }
  }

  // get regions node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("materials"));
  DOMNode* nodeMat = nodeList->item(0);
  DOMElement* elementMat = static_cast<DOMElement*>(nodeMat);

  // just loop over the children and deal with them as they come
  DOMNodeList* childern = nodeMat->getChildNodes();
  for (int i=0; i<childern->getLength(); i++) {
    bool cappressON = false;
    Teuchos::ParameterList caplist;
    std::string capname;
    DOMNode* cur = childern->item(i) ;
    if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
      attrMap = cur->getAttributes();
      nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
      if (nodeAttr) {
        textContent = XMLString::transcode(nodeAttr->getNodeValue());
      } else {
        Errors::Message msg;
        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing materials - " ;
        msg << "No attribute name found for material. \n" ;
        msg << "  Please correct and try again \n" ;
        Exceptions::amanzi_throw(msg);
      }

      Teuchos::ParameterList matlist(textContent);
      DOMNodeList* kids = cur->getChildNodes();
      for (int j=0; j<kids->getLength(); j++) {
        DOMNode* curkid = kids->item(j) ;
        if (DOMNode::ELEMENT_NODE == curkid->getNodeType()) {
            tagName  = XMLString::transcode(curkid->getNodeName());
	  if (strcmp("assigned_regions",tagName)==0){
	      //TODO: EIB - if this is more than 1 region -> assuming comma seperated list of strings????
              textContent2 = XMLString::transcode(curkid->getTextContent());
	      Teuchos::Array<std::string> regs = make_regions_list(textContent2);
	      matlist.set<Teuchos::Array<std::string> >("Assigned Regions",regs);
	      XMLString::release(&textContent2);
	      if (!compare_region_names(regs, def_list)) {
                std::cout << "Amanzi::InputTranslator: ERROR - invalid region in Materials Section" << std::endl;
                std::cout << "Amanzi::InputTranslator: valid regions are:" << std::endl;
		def_list.sublist("regions").print(std::cout,true,false);
                Exceptions::amanzi_throw(Errors::Message("Exiting due to errors in input xml file"));
	      }
	  }
          else if  (strcmp("mechanical_properties",tagName)==0){
              DOMNodeList* list = curkid->getChildNodes();
	          // loop over child: deal with porosity and density
              for (int k=0; k<list->getLength(); k++) {
                DOMNode* curkiddy = list->item(k) ;
                if (DOMNode::ELEMENT_NODE == curkiddy->getNodeType()) {
                  propName  = XMLString::transcode(curkiddy->getNodeName());
	          if  (strcmp("porosity",propName)==0){
	            // TODO: EIB - assuming value, implement file later
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for porosity. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Porosity: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("particle_density",propName)==0){
	            // TODO: EIB - assuming value, implement file later
		        // TODO: EIB - should be check value >= 0.
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for particle_density. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }
	            matlist.sublist("Particle Density: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("specific_storage",propName)==0){
		         // TODO: EIB - not handling file case
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for specific_storage. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }
	            matlist.sublist("Specific Storage: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("specific_yield",propName)==0){
		        // TODO: EIB - not handling file case
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for specific_yield. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Specific Yield: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("dispersion_tensor",propName)==0){
		        // TODO: EIB - not handling file case
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("alpha_l"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute alpha_l found for dispersion_tensor. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Dispersion Tensor: Uniform Isotropic").set<double>("alphaL",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("alpha_t"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute alpha_t found for dispersion_tensor. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Dispersion Tensor: Uniform Isotropic").set<double>("alphaT",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("molecular_diffusion",propName)==0){
		        // TODO: EIB - not handling file case
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for molecular_diffusion. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Molecular Diffusion: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          } else if  (strcmp("tortuosity",propName)==0){
		        // TODO: EIB - not handling file case
                    attrMap = curkiddy->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	              msg << "No attribute value found for tortuosity. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

	            matlist.sublist("Tortuosity: Uniform").set<double>("Value",get_double_constant(textContent2,def_list));
	            XMLString::release(&textContent2);
	          }
		}
	      }
	  }
          else if  (strcmp("permeability",tagName)==0){
	      // loop over attributes to get x,y,z
	      hasPerm = true;
	      char *x,*y,*z;
              attrMap = curkid->getAttributes();
	      Teuchos::ParameterList perm;
	      Teuchos::ParameterList permTmp;
	      for (int k=0; k<attrMap->getLength(); k++) {
                DOMNode* attrNode = attrMap->item(k) ;
                if (DOMNode::ATTRIBUTE_NODE == attrNode->getNodeType()) {
                  char* attrName = XMLString::transcode(attrNode->getNodeName());
	          if (strcmp("x",attrName)==0){
                      x = XMLString::transcode(attrNode->getNodeValue());
		      permTmp.set<double>("x",get_double_constant(x,def_list));
	              XMLString::release(&x);
		  } else if (strcmp("y",attrName)==0){
                      y = XMLString::transcode(attrNode->getNodeValue());
		      permTmp.set<double>("y",get_double_constant(y,def_list));
	              XMLString::release(&y);
		  } else if (strcmp("z",attrName)==0){
                      z = XMLString::transcode(attrNode->getNodeValue());
		      permTmp.set<double>("z",get_double_constant(z,def_list));
	              XMLString::release(&z);
		  }
		}
	      }
	      bool checkY = true , checkZ = true;
	      if (permTmp.isParameter("y")) {
		if (permTmp.get<double>("x") != permTmp.get<double>("y")) checkY = false;
	      } 
	      if (permTmp.isParameter("z")) {
		if (permTmp.get<double>("x") != permTmp.get<double>("z")) checkZ = false;
	      } 
	      if (checkY && checkZ) {
		perm.set<double>("Value",permTmp.get<double>("x"));
	        matlist.sublist("Intrinsic Permeability: Uniform") = perm;
	      } else {
		perm.set<double>("x",permTmp.get<double>("x"));
	        if (permTmp.isParameter("y")) {
		    perm.set<double>("y",permTmp.get<double>("y")) ;
		}
	        if (permTmp.isParameter("z")) {
		    perm.set<double>("z",permTmp.get<double>("z")) ;
		}
	        matlist.sublist("Intrinsic Permeability: Anisotropic Uniform") = perm;
	      }
	  }
          else if  (strcmp("hydraulic_conductivity",tagName)==0){
	      // loop over attributes to get x,y,z
	      char *x,*y,*z;
              hasHC = true;
              attrMap = curkid->getAttributes();
	      Teuchos::ParameterList hydcond;
	      Teuchos::ParameterList hydcondTmp;
	      for (int k=0; k<attrMap->getLength(); k++) {
                DOMNode* attrNode = attrMap->item(k) ;
                if (DOMNode::ATTRIBUTE_NODE == attrNode->getNodeType()) {
                  char* attrName = XMLString::transcode(attrNode->getNodeName());
	          if (strcmp("x",attrName)==0){
                      x = XMLString::transcode(attrNode->getNodeValue());
		      hydcondTmp.set<double>("x",get_double_constant(x,def_list));
	              XMLString::release(&x);
		  } else if (strcmp("y",attrName)==0){
                      y = XMLString::transcode(attrNode->getNodeValue());
		      hydcondTmp.set<double>("y",get_double_constant(y,def_list));
	              XMLString::release(&y);
		  } else if (strcmp("z",attrName)==0){
                      z = XMLString::transcode(attrNode->getNodeValue());
		      hydcondTmp.set<double>("z",get_double_constant(z,def_list));
	              XMLString::release(&z);
		  }
		}
	      }
	      bool checkY = true , checkZ = true;
	      if (hydcondTmp.isParameter("y")) {
		if (hydcondTmp.get<double>("x") != hydcondTmp.get<double>("y")) checkY = false;
	      } 
	      if (hydcondTmp.isParameter("z")) {
		if (hydcondTmp.get<double>("x") != hydcondTmp.get<double>("z")) checkZ = false;
	      } 
	      if (checkY && checkZ) {
		hydcond.set<double>("Value",hydcondTmp.get<double>("x"));
	        matlist.sublist("Hydraulic Conductivity: Uniform") = hydcond;
	      } else {
		hydcond.set<double>("x",hydcondTmp.get<double>("x"));
	        if (hydcondTmp.isParameter("y")) {
		    hydcond.set<double>("y",hydcondTmp.get<double>("y")) ;
		}
	        if (hydcondTmp.isParameter("z")) {
		    hydcond.set<double>("z",hydcondTmp.get<double>("z")) ;
		}
	        matlist.sublist("Hydraulic Conductivity: Anisotropic Uniform") = hydcond;
	      }
	  }
          else if  (strcmp("cap_pressure",tagName)==0){
              attrMap = curkid->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("model"));
	      if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	        msg << "No attribute model found for cap_pressure. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }

	      if  (strcmp("van_genuchten",textContent2)==0){
		  cappressON = true;
                  DOMNodeList* paramList= curkid->getChildNodes();
                  for (int k=0; k<paramList->getLength(); k++) {
                    DOMNode* paramNode = paramList->item(k) ;
                    if (DOMNode::ELEMENT_NODE == paramNode->getNodeType()) {
                      propName  = XMLString::transcode(paramNode->getNodeName());
	              if  (strcmp("parameters",propName)==0){
		        attrMap = paramNode->getAttributes();
		        for (int l=0; l<attrMap->getLength(); l++) {
                            DOMNode* attr = attrMap->item(l) ;
                            attrName  = XMLString::transcode(attr->getNodeName());
                            attrValue  = XMLString::transcode(attr->getNodeValue());
			    if (strcmp(attrName,"sr")==0) {
		              caplist.set<double>("Sr",get_double_constant(attrValue,def_list));
			    } else {
		              caplist.set<double>(attrName,get_double_constant(attrValue,def_list));
			    }
	                    XMLString::release(&attrName);
	                    XMLString::release(&attrValue);
			}
		      }
		    }
		  }
		  capname = "Capillary Pressure: van Genuchten";
	      } else if (strcmp("brooks_corey",textContent2)==0){
		  cappressON = true;
                  DOMNodeList* paramList= curkid->getChildNodes();
                  for (int k=0; k<paramList->getLength(); k++) {
                    DOMNode* paramNode = paramList->item(k) ;
                    if (DOMNode::ELEMENT_NODE == paramNode->getNodeType()) {
                      propName  = XMLString::transcode(paramNode->getNodeName());
	              if  (strcmp("parameters",propName)==0){
		        attrMap = paramNode->getAttributes();
		        for (int l=0; l<attrMap->getLength(); l++) {
                            DOMNode* attr = attrMap->item(l) ;
                            attrName  = XMLString::transcode(attr->getNodeName());
                            attrValue  = XMLString::transcode(attr->getNodeValue());
			    if (strcmp(attrName,"sr")==0) {
		              caplist.set<double>("Sr",get_double_constant(attrValue,def_list));
			    } else {
		              caplist.set<double>(attrName,get_double_constant(attrValue,def_list));
			    }
	                    XMLString::release(&attrName);
	                    XMLString::release(&attrValue);
			}
		      }
		    }
		  }
		  capname = "Capillary Pressure: Brooks Corey";
	      }
	      XMLString::release(&textContent2);
	  }
          else if  (strcmp("rel_perm",tagName)==0){
	      // TODO: EIB - how to handle if cappress=false? ie, caplist not setup yet?
              attrMap = curkid->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("model"));
	      if (nodeAttr) {
                textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing material - " ;
	        msg << "No attribute model found for rel_perm. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }

	      if (strcmp(textContent2,"burdine")==0) {
	        caplist.set<std::string>("Relative Permeability","Burdine");
              } else if (strcmp(textContent2,"mualem")==0) {
	        caplist.set<std::string>("Relative Permeability","Mualem");
	      }
	      if (strcmp(textContent2,"none")!=0) {
                  DOMNodeList* paramList= curkid->getChildNodes();
                  for (int k=0; k<paramList->getLength(); k++) {
                    DOMNode* paramNode = paramList->item(k) ;
                    if (DOMNode::ELEMENT_NODE == paramNode->getNodeType()) {
                      propName  = XMLString::transcode(paramNode->getNodeName());
	              if  (strcmp("optional_krel_smoothing_interval",propName)==0){
                        propValue  = XMLString::transcode(paramNode->getTextContent());
		        caplist.set<double>("krel smoothing interval",get_double_constant(propValue,def_list));
		      }
		    }
		  }
	      }

	  }
          else if  (strcmp("sorption_isotherms",tagName)==0){
            // TODO: barker - write bgd file to go with this!!!!
	    // TODO: barker - should check that chemistry is on and set to amanzi native
            Teuchos::ParameterList sorptionPL;
	    // loop over child: deal with solutes
            DOMNodeList* list = curkid->getChildNodes();
            for (int k=0; k<list->getLength(); k++) {
              DOMNode* curkiddy = list->item(k) ;
              if (DOMNode::ELEMENT_NODE == curkiddy->getNodeType()) {
                propName  = XMLString::transcode(curkiddy->getNodeName());
		// error checking to make sure this is a solute element, all properties are in attributes
	        if  (strcmp("solute",propName)==0){
                  attrMap = curkiddy->getAttributes();
                  Teuchos::ParameterList solutePL;
		  char *soluteName;
		  char *modelName;
		  // looping over attributes
	          for (int l=0; l<attrMap->getLength(); l++) {
                    DOMNode* attrNode = attrMap->item(l) ;
                    if (DOMNode::ATTRIBUTE_NODE == attrNode->getNodeType()) {
                      char* attrName = XMLString::transcode(attrNode->getNodeName());
	              if (strcmp("name",attrName)==0){
		        soluteName = XMLString::transcode(attrNode->getNodeValue());
		      } else if (strcmp("model",attrName)==0){
		        modelName = XMLString::transcode(attrNode->getNodeValue());
		      } else if (strcmp("kd",attrName)==0){
		        solutePL.set<double>("Kd",get_double_constant(XMLString::transcode(attrNode->getNodeValue()),def_list));
		      } else if (strcmp("b",attrName)==0){
		        solutePL.set<double>("Langmuir b",get_double_constant(XMLString::transcode(attrNode->getNodeValue()),def_list));
		      } else if (strcmp("n",attrName)==0){
		        solutePL.set<double>("Freundlich n",get_double_constant(XMLString::transcode(attrNode->getNodeValue()),def_list));
		      }
		    }
		  }
		  sorptionPL.sublist(soluteName) = solutePL;
		}
	      }
	    }
	    matlist.sublist("Sorption Isotherms") = sorptionPL;
	    // write BGD file
            write_BDG_file(sorptionPL, def_list);
	    // Chemistry list is also necessary - this is created under numerical controls section 
	  }
 	  XMLString::release(&tagName);
	}
      }
      if(cappressON) matlist.sublist(capname) = caplist;
      list.sublist(textContent) = matlist;
    }

  }
  if (!hasPerm and !hasHC){
    Teuchos::ParameterList perm;
    perm.set<double>("Value",0.0);
    list.sublist(textContent).sublist("Intrinsic Permeability: Uniform") = perm;
  }
  XMLString::release(&textContent);

  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_initial_conditions(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNode* nodeTmp;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* propName;
  char* textContent;
  char* textContent2;
  char* char_array;
  char* attrName;
  char* attrValue;
  char* phaseName;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Initial Conditions."<< std::endl;
    }
  }

  // get regions node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("initial_conditions"));
  DOMNode* nodeIC = nodeList->item(0);
  DOMElement* elementIC = static_cast<DOMElement*>(nodeIC);

  // just loop over the children and deal with them as they come
  DOMNodeList* childern = nodeIC->getChildNodes();
  for (int i=0; i<childern->getLength(); i++) {
    DOMNode* cur = childern->item(i) ;
    if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
      // get name of IC, then loop over it's children to fill it in
      attrMap = cur->getAttributes();
      nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
      if (nodeAttr) {
        textContent = XMLString::transcode(nodeAttr->getNodeValue());
      } else {
	Errors::Message msg;
	msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	msg << "No attribute name found for initial_condition. \n" ;
	msg << "  Please correct and try again \n" ;
	Exceptions::amanzi_throw(msg);
      }

      Teuchos::ParameterList iclist(textContent);
      DOMNodeList* IC = cur->getChildNodes();
      for (int j=0; j<IC->getLength(); j++) {
        DOMNode* ICNode = IC->item(j) ;
        tagName  = XMLString::transcode(ICNode->getNodeName());
        //NOTE: EIB - ignoring comments for now
        if (strcmp(tagName,"assigned_regions")==0) {
	  //TODO: EIB - if this is more than 1 region -> assuming comma seperated list of strings????
          textContent2 = XMLString::transcode(ICNode->getTextContent());
	  Teuchos::Array<std::string> regs = make_regions_list(textContent2);
	  iclist.set<Teuchos::Array<std::string> >("Assigned Regions",regs);
	  XMLString::release(&textContent2);
	  if (!compare_region_names(regs, def_list)) {
                std::cout << "Amanzi::InputTranslator: ERROR - invalid region in Initial Conditions Section" << std::endl;
                std::cout << "Amanzi::InputTranslator: valid regions are:" << std::endl;
		def_list.sublist("regions").print(std::cout,true,false);
                Exceptions::amanzi_throw(Errors::Message("Exiting due to errors in input xml file"));
	  }
        }
        else if (strcmp(tagName,"liquid_phase")==0) {
          //TODO: EIB - deal with liquid phase
          attrMap = ICNode->getAttributes();
          nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	  if (nodeAttr) {
            phaseName = XMLString::transcode(nodeAttr->getNodeValue());
	  } else {
	    Errors::Message msg;
	    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	    msg << "No attribute name found for liquid_phase. \n" ;
	    msg << "  Please correct and try again \n" ;
	    Exceptions::amanzi_throw(msg);
	  }

	  // loop over children, deal with liquid_component, solute_component, geomchemistry
          DOMNodeList* compList = ICNode->getChildNodes();
          for (int k=0; k<compList->getLength(); k++) {
            //TODO: EIB - add ELEMENT check
            DOMNode* compNode = compList->item(k) ;
            char* compName  = XMLString::transcode(compNode->getNodeName());
            if (strcmp(compName,"liquid_component")==0) {
	      // loop over children to find pressure
              DOMNodeList* childList = compNode->getChildNodes();
              for (int l=0; l<childList->getLength(); l++) {
                DOMNode* pressure = childList->item(l) ;
                char* pressName  = XMLString::transcode(pressure->getNodeName());
	        Teuchos::ParameterList pressureList;
	        pressureList.set<std::string>("Phase","Aqueous");
                if (strcmp(pressName,"uniform_pressure")==0 || strcmp(pressName,"uniform_saturation")==0) {
	          // loop over attributes to get info
	          attrMap = pressure->getAttributes();
                  nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		  if (nodeAttr) {
                    attrValue = XMLString::transcode(nodeAttr->getNodeValue());
	          } else {
	            Errors::Message msg;
	            msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	            msg << "No attribute value found for uniform_pressure or uniform_saturation. \n" ;
	            msg << "  Please correct and try again \n" ;
	            Exceptions::amanzi_throw(msg);
	          }

	          pressureList.set<double>("Value",get_double_constant(attrValue,def_list));
	          XMLString::release(&attrValue);
                  if (strcmp(pressName,"uniform_pressure")==0 ) {
		      iclist.sublist("IC: Uniform Pressure") = pressureList;
		  } else {
		      iclist.sublist("IC: Uniform Saturation") = pressureList;
		  }
		}
		else if (strcmp(pressName,"linear_pressure")==0 || strcmp(pressName,"linear_saturation")==0) {
	            char* char_array;
		    //value
	            attrMap = pressure->getAttributes();
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	              msg << "No attribute value found for linear_pressure or linear_saturation. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }

		    pressureList.set<double>("Reference Value",get_double_constant(attrValue,def_list));
	            XMLString::release(&attrValue);
		    //reference_coord
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("reference_coord"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	              msg << "No attribute reference_coord found for linear_pressure or linear_saturation. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }
	            Teuchos::Array<double> coord = make_coordinates(attrValue, def_list);
		    pressureList.set<Teuchos::Array<double> >("Reference Coordinate",coord);
	            XMLString::release(&attrValue);
		    //gradient
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("gradient"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
	            } else {
	              Errors::Message msg;
	              msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	              msg << "No attribute gradient found for linear_pressure or linear_saturation. \n" ;
	              msg << "  Please correct and try again \n" ;
	              Exceptions::amanzi_throw(msg);
	            }
	            Teuchos::Array<double> grad = make_coordinates(attrValue, def_list);
		    pressureList.set<Teuchos::Array<double> >("Gradient Value",grad);
	            XMLString::release(&attrValue);
                    if (strcmp(pressName,"linear_pressure")==0 ) {
		      iclist.sublist("IC: Linear Pressure") = pressureList;
		    } else {
		      iclist.sublist("IC: Linear Saturation") = pressureList;
		    }
		}
		else if (strcmp(pressName,"velocity")==0 ) {
		    Teuchos::Array<double> vel_vector;
	            attrMap = pressure->getAttributes();
		    // get velocity vector
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("x"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
		      vel_vector.append(get_double_constant(attrValue, def_list));
	              XMLString::release(&attrValue);
		    }
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("y"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
		      vel_vector.append(get_double_constant(attrValue, def_list));
	              XMLString::release(&attrValue);
		    }
                    nodeAttr = attrMap->getNamedItem(XMLString::transcode("z"));
		    if (nodeAttr) {
                      attrValue = XMLString::transcode(nodeAttr->getNodeValue());
		      vel_vector.append(get_double_constant(attrValue, def_list));
	              XMLString::release(&attrValue);
		    }
		    // check that vector dimension == problem dimension
		    if (vel_vector.length() != dimension_) {
		      Errors::Message msg;
		      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
		      msg << "velocity vectory size does not match the spatial dimension of the problem. \n" ;
		      msg << "  Please correct and try again \n" ;
		      Exceptions::amanzi_throw(msg);
		    }
		    // set up new element
		    // TODO:: EIB - does function="uniform | linear" translate to IC: Uniform Velocity and IC: Linear Velocity???
		    //              Linear Velocity does not exist => need error message as such
		    pressureList.set<Teuchos::Array<double> >("Velocity Vector",vel_vector);
		    iclist.sublist("IC: Uniform Velocity") = pressureList;

		}
	      }
			      
	    }
	    else if (strcmp(compName,"solute_component")==0) {
	      char* solName;
	      char* funcType;
	      Teuchos::ParameterList sclist;
	      attrMap = compNode->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	      if (nodeAttr) {
                solName = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	        msg << "No attribute name found for solute_component. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }

	      attrMap = compNode->getAttributes();
              nodeAttr = attrMap->getNamedItem(XMLString::transcode("function"));
	      if (nodeAttr) {
                funcType = XMLString::transcode(nodeAttr->getNodeValue());
	      } else {
	        Errors::Message msg;
	        msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	        msg << "No attribute function found for solute_component. \n" ;
	        msg << "  Please correct and try again \n" ;
	        Exceptions::amanzi_throw(msg);
	      }
	      if (strcmp(funcType,"uniform")==0){
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("value"));
		if (nodeAttr) {
                  textContent2 = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing initial_conditions - " ;
	          msg << "No attribute value found for solute_component. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }

		sclist.sublist("IC: Uniform Concentration").set<double>("Value",get_double_constant(textContent2,def_list));
	        XMLString::release(&textContent2);
	      }else if (strcmp(funcType,"linear")==0){
		// TODO: EIB - currently can't handle this
	      }
	      //TODO: EIB - not added concerntation units, confused by what to add. grab from units?
	      iclist.sublist("Solute IC").sublist("Aqueous").sublist(phaseName).sublist(solName) = sclist;
	      XMLString::release(&solName);
	      XMLString::release(&funcType);
	    }
	    else if (strcmp(compName,"geochemistry")==0) {
              //TODO: EIB - deal with geochemisty later
	    }
	  }
	  XMLString::release(&phaseName);
        }
        else if (strcmp(tagName,"solid_phase")==0) {
          //TODO: EIB - deal with solid phase -> mineral, geochemisty
        }
        XMLString::release(&tagName);
      }
      list.sublist(textContent) = iclist;
      XMLString::release(&textContent);
    }
  }

  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_boundary_conditions(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNodeList* BCList;
  DOMNode* nodeTmp;
  DOMNode* nodeAttr;
  DOMNamedNodeMap* attrMap;
  char* tagName;
  char* propName;
  char* phaseName;
  char* textContent;
  char* textContent2;
  char* char_array;
  char* attrName;
  char* attrValue;
  Errors::Message msg;


  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Boundary Conditions."<< std::endl;
    }
  }

  // get BCs node
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("boundary_conditions"));

  if (nodeList->getLength() > 0 ){ // boundary conditions tag does not have to exist

  DOMNode* nodeBC = nodeList->item(0);
  DOMElement* elementBC = static_cast<DOMElement*>(nodeBC);

  // get list of BCs
  BCList = elementBC->getElementsByTagName(XMLString::transcode("boundary_condition"));
  for (int i=0; i<BCList->getLength(); i++) {
    DOMNode* cur = BCList->item(i) ;
    if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
      // get name of BC, then loop over it's children to fill it in
      attrMap = cur->getAttributes();
      nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
      if (nodeAttr) {
        textContent = XMLString::transcode(nodeAttr->getNodeValue());
      } else {
	Errors::Message msg;
	msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions - " ;
	msg << "No attribute name found for boundary_condition. \n" ;
	msg << "  Please correct and try again \n" ;
	Exceptions::amanzi_throw(msg);
      }

      Teuchos::ParameterList bclist(textContent);
      DOMNodeList* BC = cur->getChildNodes();
      for (int j=0; j<BC->getLength(); j++) {
        DOMNode* BCNode = BC->item(j) ;
        tagName  = XMLString::transcode(BCNode->getNodeName());
        //NOTE: EIB - ignoring comments for now
        if (strcmp(tagName,"assigned_regions")==0) {
	  //TODO: EIB - if this is more than 1 region -> assuming comma seperated list of strings????
          textContent2 = XMLString::transcode(BCNode->getTextContent());
	  Teuchos::Array<std::string> regs = make_regions_list(textContent2);
	  bclist.set<Teuchos::Array<std::string> >("Assigned Regions",regs);
	  XMLString::release(&textContent2);
	  if (!compare_region_names(regs, def_list)) {
                std::cout << "Amanzi::InputTranslator: ERROR - invalid region in Boundary Conditions Section" << std::endl;
                std::cout << "Amanzi::InputTranslator: valid regions are:" << std::endl;
		def_list.sublist("regions").print(std::cout,true,false);
                Exceptions::amanzi_throw(Errors::Message("Exiting due to errors in input xml file"));
	  }
        }
        else if (strcmp(tagName,"liquid_phase")==0) {
          //TODO: EIB - deal with liquid phase
          attrMap = BCNode->getAttributes();
          nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	  if (nodeAttr) {
            phaseName = XMLString::transcode(nodeAttr->getNodeValue());
	  } else {
	    Errors::Message msg;
	    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions - " ;
	    msg << "No attribute name found for liquid_phase. \n" ;
	    msg << "  Please correct and try again \n" ;
	    Exceptions::amanzi_throw(msg);
	  }

	  // loop over children, deal with liquid_component, solute_component, geomchemistry
          DOMNodeList* compList = BCNode->getChildNodes();
          for (int k=0; k<compList->getLength(); k++) {
            DOMNode* compNode = compList->item(k) ;
            char* compName  = XMLString::transcode(compNode->getNodeName());

            if (strcmp(compName,"liquid_component")==0) {
	      Teuchos::Array<double> vals;
	      Teuchos::Array<double> times;
	      Teuchos::Array<std::string> funcs;
	      std::string bcname;
	      std::string valname;
	      bool hasCoordsys = false;
	      std::string coordsys = "Absolute";

	      // loop over children and deal with bc's
              DOMNodeList* bcChildList = compNode->getChildNodes();
              for (int l=0; l<bcChildList->getLength(); l++) {
                DOMNode* bcChildNode = bcChildList->item(l) ;
                if (DOMNode::ELEMENT_NODE == bcChildNode->getNodeType()) {
                 char* bcChildName  = XMLString::transcode(bcChildNode->getNodeName());
		 std::string function;
		 double value;
		 double time;

		 if (strcmp(bcChildName,"seepage_face")==0){
		  bcname = "BC: Seepage";
		  valname = "Inward Mass Flux";
                  DOMElement* bcElem = static_cast<DOMElement*>(bcChildNode);
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("function")));
		  if (strcmp(textContent2,"linear")==0) {
		    function = "Linear";
		  } else if (strcmp(textContent2,"constant")==0) {
		    function = "Constant";
		  } else if (strcmp(textContent2,"uniform")==0) {
		    function = "Uniform";
		  }
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("inward_mass_flux")));
                  value = get_time_value(textContent2, def_list);
		  //vals.append(time);
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("start")));
                  time = get_time_value(textContent2, def_list);
		  //times.append(time);
		 } else if (strcmp(bcChildName,"no_flow")==0) {
		  bcname = "BC: Zero Flow";
                  DOMElement* bcElem = static_cast<DOMElement*>(bcChildNode);
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("function")));
		  if (strcmp(textContent2,"linear")==0) {
		    function = "Linear";
		  } else if (strcmp(textContent2,"constant")==0) {
		    function = "Constant";
		  } else if (strcmp(textContent2,"uniform")==0) {
		    function = "Uniform";
		  }
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("start")));
                  time = get_time_value(textContent2, def_list);
		  //times.append(time);
		 } else {
                  DOMElement* bcElem = static_cast<DOMElement*>(bcChildNode);
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("function")));
		  if (strcmp(textContent2,"linear")==0) {
		    function = "Linear";
		  } else if (strcmp(textContent2,"constant")==0) {
		    function = "Constant";
		  } else if (strcmp(textContent2,"uniform")==0) {
		    function = "Uniform";
		  }
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("value")));
                  value = get_double_constant(textContent2, def_list);
		  //vals.append(value);
                  textContent2 = XMLString::transcode(bcElem->getAttribute(
		                   XMLString::transcode("start")));
                  time = get_time_value(textContent2, def_list);
		  //times.append(time);

		  // translate boundary condition name here
		  if (strcmp(bcChildName,"inward_mass_flux")==0) {
		    bcname = "BC: Flux";
		    valname = "Inward Mass Flux";
		  } else if (strcmp(bcChildName,"inward_volumetric_flux")==0) {
		    bcname = "BC: Flux";
		    valname = "Outward Volumetric Flux";
		    } else if (strcmp(bcChildName,"outward_mass_flux")==0) {
		    bcname = "BC: Flux";
		    valname = "Inward Mass Flux";
		  } else if (strcmp(bcChildName,"outward_volumetric_flux")==0) {
		    bcname = "BC: Flux";
		    valname = "Outward Volumetric Flux";
		  } else if (strcmp(bcChildName,"uniform_pressure")==0) {
		    bcname = "BC: Uniform Pressure";
		    valname = "Values";
		  } else if (strcmp(bcChildName,"linear_pressure")==0) {
		    bcname = "BC: Linear Pressure";
		    valname = "Values";
		  } else if (strcmp(bcChildName,"hydrostatic")==0) {
		    bcname = "BC: Hydrostatic";
		    valname = "Water Table Height";
		    //TODO: EIB - update if necessary
		    if (bcElem->hasAttribute(XMLString::transcode("coordinate_system"))) {
                      textContent2 = XMLString::transcode(bcElem->getAttribute(
		                       XMLString::transcode("coordinate_system")));
		      hasCoordsys = true;
		      if (strcmp(textContent2,"relative to mesh top")==0){
			coordsys = "Relative";
		      }
		    }
		   }
		  }

		  // put time, function, value in sorted order
		  if (times.length() == 0) {               // if first time through
		    times.append(time);
		    funcs.append(function);
		    vals.append(value);
		  } else {
		    if (time >= times[times.length()-1]) { // if already sorted
		      times.append(time);
		      funcs.append(function);
		      vals.append(value);
		    } else {                              // otherwise, sort
		      int idx = times.length()-1;
		      Teuchos::Array<double> hold_times;
		      Teuchos::Array<double> hold_vals;
		      Teuchos::Array<std::string> hold_funcs;
		      hold_times.append(times[idx]);
		      hold_vals.append(vals[idx]);
		      hold_funcs.append(funcs[idx]);
		      times.remove(idx);
		      vals.remove(idx);
		      funcs.remove(idx);
		      idx--;
		      while (time < times[idx]) {
		        hold_times.append(times[idx]);
		        hold_vals.append(vals[idx]);
		        hold_funcs.append(funcs[idx]);
		        times.remove(idx);
		        vals.remove(idx);
		        funcs.remove(idx);
		        idx--;
		      }
		      times.append(time);
		      vals.append(value);
		      funcs.append(function);
		      for (int i=0; i<hold_times.length(); i++) {
		        idx = hold_times.length()-1-i;
		        times.append(hold_times[hold_times.length()-idx]);
		        vals.append(hold_vals[hold_times.length()-idx]);
		        funcs.append(hold_funcs[hold_times.length()-idx]);
		      }
		    }
		  }
		}
	      }
	      // if len array == 1: add dummy vals to create and interval
	      if (times.length()==1 && bcname != "BC: Zero Flow" ){
		// EIB: thought this would work, simple version doesn't in the steady case where end time = 0
		//      overkill to do correctly
                //if (def_list.sublist("simulation").isParameter("simulation_end")) {
		//  double end_time = (def_list.sublist("simulation").get<double>("simulation_end");
		//  if (end_time > (times[0])) {  
		//    times.append(end_time);
		//  } else {
		//    times.append(times[0]+1.);
		//  }
		//}
		//times.append(times[0]+1.);
		if (def_list.sublist("simulation").isParameter("simulation_end")) {
	  	    times.append(def_list.sublist("simulation").get<double>("simulation_end")+1.);
		} else { 
	  	    times.append(times[0]+1.);
		}
		vals.append(vals[0]);
	      }
	      //EIB - this is iffy!!! Talked with Ellen, this is consistent with her assumptions in Akuna, for now
	      if (times.length()==funcs.length()) funcs.remove(funcs.length()-1); 

	      // create a BC new list here
	      Teuchos::ParameterList newbclist;
	      newbclist.set<Teuchos::Array<double> >("Times",times);
	      newbclist.set<Teuchos::Array<std::string> >("Time Functions",funcs);
	      if (bcname != "BC: Zero Flow") newbclist.set<Teuchos::Array<double> >(valname,vals);
	      if (bcname == "BC: Hydrostatic" && hasCoordsys) newbclist.set<std::string>("Coordinate System",coordsys);
	      bclist.sublist(bcname) = newbclist;
	    }
             if (strcmp(compName,"solute_component")==0) {
	      char* solName;
	      //Teuchos::ParameterList sclist;
	      // loop over elements to build time series, add to list
	      Teuchos::Array<double> vals;
	      Teuchos::Array<double> times;
	      Teuchos::Array<std::string> funcs;

	      // EIB - loop over elements, aqueous_conc, to get all. Could have multiple time steps for multiple components
	      Teuchos::ParameterList sc_tmplist;
               DOMNodeList* acList = compNode->getChildNodes();
               for (int l=0; l<acList->getLength(); l++) {
                 DOMNode* cur = acList->item(l) ;
                 if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
		  std::string function;
		  double value;
		  double time;
                    DOMElement* bcElem = static_cast<DOMElement*>(cur);
		  if (strcmp(XMLString::transcode(bcElem->getTagName()),"aqueous_conc") != 0) {
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions->solute_component - " ;
		     msg << "aqueous_conc was missing or ill-formed. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
                    solName = XMLString::transcode(bcElem->getAttribute(XMLString::transcode("name")));
		  if (solName[0] == '\0'){
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions->solute_component - " ;
		     msg << "aqueous_conc name was missing or ill-formed. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
                    textContent2 = XMLString::transcode(bcElem->getAttribute(XMLString::transcode("function")));
		  if (textContent2[0] == '\0'){
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions->solute_component - " ;
		     msg << "aqueous_conc function was missing or ill-formed. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
		  if (strcmp(textContent2,"linear")==0) {
		    function = "Linear";
		  } else if (strcmp(textContent2,"constant")==0) {
		    function = "Constant";
		  // EIB - uniform is a space option, not a time option
		  } 
                    textContent2 = XMLString::transcode(bcElem->getAttribute(XMLString::transcode("value")));
		  if (textContent2[0] == '\0'){
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions->solute_component - " ;
		     msg << "aqueous_conc value was missing or ill-formed. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
                    value = get_time_value(textContent2, def_list);
                    textContent2 = XMLString::transcode(bcElem->getAttribute(XMLString::transcode("start")));
		  if (textContent2[0] == '\0'){
		     msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing boundary_conditions->solute_component - " ;
		     msg << "aqueous_conc start was missing or ill-formed. \n  Please correct and try again \n" ;
		     Exceptions::amanzi_throw(msg);
		  }
                    time = get_time_value(textContent2, def_list);
		  if ( sc_tmplist.isParameter(solName)) {
		     Teuchos::Array<double> tmpV;
		     Teuchos::Array<double> tmpT;
		     Teuchos::Array<std::string> tmpF;
		     tmpV = sc_tmplist.sublist(solName).get<Teuchos::Array<double> >("value");
		     tmpV.append(value);
		     sc_tmplist.sublist(solName).set<Teuchos::Array<double> >("value",tmpV);
		     tmpT = sc_tmplist.sublist(solName).get<Teuchos::Array<double> >("time");
		     tmpT.append(time);
		     sc_tmplist.sublist(solName).set<Teuchos::Array<double> >("time",tmpT);
		     tmpF = sc_tmplist.sublist(solName).get<Teuchos::Array<std::string> >("function");
		     tmpF.append(function);
		     sc_tmplist.sublist(solName).set<Teuchos::Array<std::string> >("function",tmpF);
		  } else {
		     Teuchos::ParameterList tmpsc_list;
		     Teuchos::Array<double> tmpV;
		     Teuchos::Array<double> tmpT;
		     Teuchos::Array<std::string> tmpF;
		     tmpV.append(value);
		     tmpsc_list.set<Teuchos::Array<double> >("value",tmpV);
		     tmpT.append(time);
		     tmpsc_list.set<Teuchos::Array<double> >("time",tmpT);
		     tmpF.append(function);
		     tmpsc_list.set<Teuchos::Array<std::string> >("function",tmpF);
		     sc_tmplist.sublist(solName) = tmpsc_list;
		  }
		}
	      }
	      // EIB - now do time sorting, if have multiple time steps for each component
               for (Teuchos::ParameterList::ConstIterator i = sc_tmplist.begin(); i != sc_tmplist.end(); i++) {
                    Teuchos::ParameterList& curComp_list = sc_tmplist.sublist(sc_tmplist.name(i)) ;
                    Teuchos::Array<double> times = curComp_list.get<Teuchos::Array<double> >("time") ;
                    Teuchos::Array<double> values = curComp_list.get<Teuchos::Array<double> >("value") ;
                    Teuchos::Array<std::string> funcs = curComp_list.get<Teuchos::Array<std::string> >("function") ;
                    Teuchos::Array<double> sort_vals;
                    Teuchos::Array<double> sort_times;
		  Teuchos::Array<std::string> sort_func;
                    for (int j=0; j<times.length(); j++) {
		      if (j==0) {
		          sort_times.append(times[j]);
		          sort_func.append(funcs[j]);
		          sort_vals.append(values[j]);
		      } else {
			  if (times[j] >= sort_times[sort_times.length()-1]) { //if already sorted
			      sort_times.append(times[j]);
			      sort_func.append(funcs[j]);
			      sort_vals.append(values[j]);
			  } else {                                            // otherwise sort
		              int idx = times.length()-1;
		              Teuchos::Array<double> hold_times;
		              Teuchos::Array<double> hold_vals;
		              Teuchos::Array<std::string> hold_funcs;
		              hold_times.append(sort_times[idx]);
		              hold_vals.append(sort_vals[idx]);
		              hold_funcs.append(sort_func[idx]);
		              sort_times.remove(idx);
		              sort_vals.remove(idx);
		              sort_func.remove(idx);
		              idx--;
		              while (times[j] < sort_times[idx]) {
		                hold_times.append(sort_times[idx]);
		                hold_vals.append(sort_vals[idx]);
		                hold_funcs.append(sort_func[idx]);
		                sort_times.remove(idx);
		                sort_vals.remove(idx);
		                sort_func.remove(idx);
		                idx--;
		              }
		              sort_times.append(times[j]);
		              sort_vals.append(values[j]);
		              sort_func.append(funcs[j]);
		              for (int i=0; i<hold_times.length(); i++) {
		                idx = hold_times.length()-1-i;
		                sort_times.append(hold_times[hold_times.length()-idx]);
		                sort_vals.append(hold_vals[hold_times.length()-idx]);
		                sort_func.append(hold_funcs[hold_times.length()-idx]);
		              }
			  }
		      }
		  }
                   // if len array == 1: add dummy vals to create and interval
                   if (sort_times.length()==1){
                       //sort_times.append(sort_times[0]+1.);
		       if (def_list.sublist("simulation").isParameter("simulation_end")) {
	  	         sort_times.append(def_list.sublist("simulation").get<double>("simulation_end")+1.);
		       } else { 
	  	         sort_times.append(times[0]+1.);
		       }
                       sort_vals.append(sort_vals[0]);
	          }
                   //EIB - this is iffy!!! Talked with Ellen, this is consistent with her assumptions in Akuna, for now
                   if (sort_times.length()==sort_func.length()) sort_func.remove(sort_func.length()-1);
                   // EIB - add these new sorted arrays back to PL
                   sc_tmplist.sublist(sc_tmplist.name(i)).set<Teuchos::Array<double> >("sorted_times",sort_times);
                   sc_tmplist.sublist(sc_tmplist.name(i)).set<Teuchos::Array<double> >("sort_values",sort_vals);
                   sc_tmplist.sublist(sc_tmplist.name(i)).set<Teuchos::Array<std::string> >("sorted_functions",sort_func);
	      }
	      //TODO: EIB - not added concerntation units, need to grab from units
               // EIB - now add each solute BC to PL
               for (Teuchos::ParameterList::ConstIterator i = sc_tmplist.begin(); i != sc_tmplist.end(); i++) {
                   Teuchos::ParameterList sclist;
                   Teuchos::ParameterList& curComp_list = sc_tmplist.sublist(sc_tmplist.name(i)) ;
	          sclist.sublist("BC: Uniform Concentration").set<Teuchos::Array<double> >("Times",curComp_list.get<Teuchos::Array<double> >("sorted_times"));
	          sclist.sublist("BC: Uniform Concentration").set<Teuchos::Array<std::string> >("Time Functions",curComp_list.get<Teuchos::Array<std::string> >("sorted_functions"));
	          sclist.sublist("BC: Uniform Concentration").set<Teuchos::Array<double> >("Values",curComp_list.get<Teuchos::Array<double> >("sort_values"));
	          bclist.sublist("Solute BC").sublist("Aqueous").sublist(phaseName).sublist(sc_tmplist.name(i)) = sclist;
               }
	      XMLString::release(&solName);
	    }
             if (strcmp(compName,"geochemistry")==0) {
              //TODO: EIB - deal with geochemisty later
	    }
	  }
          XMLString::release(&phaseName);
        }
        else if (strcmp(tagName,"solid_phase")==0) {
          //TODO: EIB - deal with solid phase -> mineral, geochemisty
        }
        XMLString::release(&tagName);
      }
      list.sublist(textContent) = bclist;
      XMLString::release(&textContent);
    }
  }

  }

  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_sources(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
      std::cout << "Amanzi::InputTranslator: Getting Sources."<< std::endl;
    }
  }

  // get Sources node
  DOMNodeList* nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("sources"));
  if (nodeList->getLength() > 0) {
  DOMNode* nodeSC = nodeList->item(0);
  DOMElement* elementSC = static_cast<DOMElement*>(nodeSC);

  // get list of Source
  DOMNodeList* SCList = elementSC->getElementsByTagName(XMLString::transcode("source"));
  std::string phase("Aqueous"); //NOTE:: EIB: currently only support this, add checks later
  std::string component("Water");

  for (int i=0; i<SCList->getLength(); i++) {
    DOMNode* cur = SCList->item(i) ;

    if (DOMNode::ELEMENT_NODE == cur->getNodeType()) {
      char* textContent;
      DOMNamedNodeMap* attrMap = cur->getAttributes();
      DOMNode* nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
      if (nodeAttr) {
        textContent = XMLString::transcode(nodeAttr->getNodeValue());
      } else {
	Errors::Message msg;
	msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing sources - " ;
	msg << "No attribute name found for source. \n" ;
	msg << "  Please correct and try again \n" ;
	Exceptions::amanzi_throw(msg);
      }

      Teuchos::ParameterList sclist;
      DOMNodeList* SC = cur->getChildNodes();
      for (int j=0; j<SC->getLength(); j++) {
        DOMNode* SCNode = SC->item(j) ;

        if (DOMNode::ELEMENT_NODE == SCNode->getNodeType()) {
          char* tagName  = XMLString::transcode(SCNode->getNodeName());
          if (strcmp(tagName,"assigned_regions")==0) {
            char* textContent2 = XMLString::transcode(SCNode->getTextContent());
	    Teuchos::Array<std::string> regs = make_regions_list(textContent2);
	    sclist.set<Teuchos::Array<std::string> >("Assigned Regions",regs);
	    XMLString::release(&textContent2);
	    if (!compare_region_names(regs, def_list)) {
		Errors::Message msg;
                msg << "Amanzi::InputTranslator: ERROR - invalid region in Sources Section" ;
                msg << "Amanzi::InputTranslator: valid regions are:" ;
		def_list.sublist("regions").print(std::cout,true,false);
		msg << "Exiting due to errors in input xml file" ;
                Exceptions::amanzi_throw(msg);
	    }
          } else if (strcmp(tagName,"liquid_phase")==0) {
            attrMap = SCNode->getAttributes();
            nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
            char* phaseName;
	    if (nodeAttr) {
              phaseName = XMLString::transcode(nodeAttr->getNodeValue());
            } else {
	      Errors::Message msg;
      	      msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing sources - " ;
	      msg << "No attribute name found for liquid_phase. \n" ;
	      msg << "  Please correct and try again \n" ;
	      Exceptions::amanzi_throw(msg);
            }

            DOMNodeList* compList = SCNode->getChildNodes();
            for (int k=0; k<compList->getLength(); k++) {
	      DOMNode* compNode = compList->item(k) ;
              char* compName  = XMLString::transcode(compNode->getNodeName());
              DOMNamedNodeMap* attrMap2 = compNode->getAttributes();
              if (strcmp(compName,"liquid_component")==0) {
	        Teuchos::Array<double> vals;
	        Teuchos::Array<double> times;
	        Teuchos::Array<std::string> funcs;
	        std::string scname;
		// get component name
                attrMap = compNode->getAttributes();
                nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
                char* compName2 ;
		if (nodeAttr) {
                  compName2 = XMLString::transcode(nodeAttr->getNodeValue());
		  component = std::string(compName2);
                } else {
	          Errors::Message msg;
      	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing sources - " ;
	          msg << "No attribute name found for liquid_component. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
                }

		// loop over children
                DOMNodeList* scChildList = compNode->getChildNodes();
                for (int l=0; l<scChildList->getLength(); l++) {
                  DOMNode* scChildNode = scChildList->item(l) ;
                  if (DOMNode::ELEMENT_NODE == scChildNode->getNodeType()) {
                    char* scChildName  = XMLString::transcode(scChildNode->getNodeName());
		    if (strcmp(scChildName,"volume_weighted")==0){
		     scname = "Source: Volume Weighted";
		    } else if (strcmp(scChildName,"permeability_weighted")==0){
		     scname = "Source: Permeability Weighted";
		    }
		    // loop over any attributes that may exist
                    DOMNamedNodeMap* attrMap2 = scChildNode->getAttributes();
                    for (int l=0; l<attrMap2->getLength(); l++) {
                      DOMNode* attrNode = attrMap2->item(l) ;
                      if (DOMNode::ATTRIBUTE_NODE == attrNode->getNodeType()) {
                        char* attrName = XMLString::transcode(attrNode->getNodeName());
                        char* attrValue = XMLString::transcode(attrNode->getNodeValue());
			if (strcmp(attrName,"function")==0) {
		          if (strcmp(attrValue,"linear")==0) {
		            funcs.append("Linear");
		          } else if (strcmp(attrValue,"constant")==0) {
		            funcs.append("Constant");
		          } else if (strcmp(attrValue,"uniform")==0) {
		            funcs.append("Uniform");
		          }
			} else if (strcmp(attrName,"start")==0) {
		          times.append(get_time_value(attrValue, def_list));
			} else if (strcmp(attrName,"value")==0) {
		          vals.append(get_time_value(attrValue, def_list));
			}
		      }
		    }
		  }
		}
	        if (times.length()==1 ){
	  	  //times.append(times[0]+1.);
		  if (def_list.sublist("simulation").isParameter("simulation_end")) {
	  	    times.append(def_list.sublist("simulation").get<double>("simulation_end")+1.);
		  } else { 
	  	    times.append(times[0]+1.);
		  }
		  vals.append(vals[0]);
	        }
	        //EIB - this is iffy!!! Talked with Ellen, this is consistent with her assumptions in Akuna, for now
	        if (times.length()==funcs.length() && funcs.length()>0) funcs.remove(funcs.length()-1); 
	        Teuchos::ParameterList newsclist;
		if (times.length() > 0) {
	          newsclist.set<Teuchos::Array<double> >("Times",times);
	          newsclist.set<Teuchos::Array<std::string> >("Time Functions",funcs);
	          newsclist.set<Teuchos::Array<double> >("Values",vals);
		}
	        sclist.sublist(scname) = newsclist;
	      } else if (strcmp(compName,"solute_component")==0) {
	        Teuchos::Array<double> vals;
	        Teuchos::Array<double> times;
	        Teuchos::Array<std::string> funcs;
	        std::string scname;
	        std::string soluteName;
                DOMNodeList* scChildList = compNode->getChildNodes();
                for (int l=0; l<scChildList->getLength(); l++) {
                  DOMNode* scChildNode = scChildList->item(l) ;
                  if (DOMNode::ELEMENT_NODE == scChildNode->getNodeType()) {
                    char* scChildName  = XMLString::transcode(scChildNode->getNodeName());
		    if (strcmp(scChildName,"flow_weighted_conc")==0){
		     scname = "Source: Flow Weighted Concentration";
		    } else if (strcmp(scChildName,"uniform_conc")==0){
		     scname = "Source: Uniform Concentration";
		    }
		    // loop over any attributes that may exist
                    DOMNamedNodeMap* attrMap2 = scChildNode->getAttributes();
                    for (int l=0; l<attrMap2->getLength(); l++) {
                      DOMNode* attrNode = attrMap2->item(l) ;
                      if (DOMNode::ATTRIBUTE_NODE == attrNode->getNodeType()) {
                        char* attrName = XMLString::transcode(attrNode->getNodeName());
                        char* attrValue = XMLString::transcode(attrNode->getNodeValue());
			if (strcmp(attrName,"function")==0) {
		          if (strcmp(attrValue,"linear")==0) {
		            funcs.append("Linear");
		          } else if (strcmp(attrValue,"constant")==0) {
		            funcs.append("Constant");
		          } else if (strcmp(attrValue,"uniform")==0) {
		            funcs.append("Uniform");
		          }
			} else if (strcmp(attrName,"start")==0) {
		          times.append(get_time_value(attrValue, def_list));
			} else if (strcmp(attrName,"value")==0) {
		          vals.append(get_time_value(attrValue, def_list));
			} else if (strcmp(attrName,"name")==0) {
		          soluteName = attrValue;
			}
                        XMLString::release(&attrName);
                        XMLString::release(&attrValue);
		      }
		    }
		  }
		}
	        if (times.length()==1 ){
		  if (def_list.sublist("simulation").isParameter("simulation_end")) {
	  	    times.append(def_list.sublist("simulation").get<double>("simulation_end")+1.);
		  } else { 
	  	    times.append(times[0]+1.);
		  }
		  vals.append(vals[0]);
	        }
	        //EIB - this is iffy!!! Talked with Ellen, this is consistent with her assumptions in Akuna, for now
	        if (times.length()==funcs.length() && funcs.length()>0) funcs.remove(funcs.length()-1); 
	        Teuchos::ParameterList newsclist;
	          newsclist.set<Teuchos::Array<double> >("Times",times);
	          newsclist.set<Teuchos::Array<std::string> >("Time Functions",funcs);
	          newsclist.set<Teuchos::Array<double> >("Values",vals);
	        //sclist.sublist(scname) = newsclist;
	        sclist.sublist("Solute SOURCE").sublist(phase).sublist(component).sublist(soluteName).sublist(scname) = newsclist;
	      }
	    }
            XMLString::release(&phaseName);
	  }
	}
      }
      list.sublist(textContent) = sclist;
      XMLString::release(&textContent);
    }
  }
  }

  return list;
  
}


/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */
Teuchos::ParameterList get_output(DOMDocument* xmlDoc, Teuchos::ParameterList def_list) {

  Teuchos::ParameterList list;

  DOMNodeList* nodeList;
  DOMNodeList* visList;
  DOMNodeList* chkList;
  DOMNodeList* obsList;
  DOMNodeList* tmpList;
  DOMNamedNodeMap* attrMap;
  DOMNode* tmpNode;
  DOMNode* nodeAttr;
  char* textContent;
  char* textContent2;


  if (def_list.sublist("simulation").isParameter("verbosity")) {
    std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
    if (verbosity == "extreme") {
	    std::cout << "Amanzi::InputTranslator: Getting Outputs."<< std::endl;
    }
  }

  // get definitions node - this node MAY exist ONCE
  // this contains any time macros and cycle macros
  // they are stored in the outputs of the old format
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("definitions"));
  if (nodeList->getLength() > 0) {
    DOMNode* defNode = nodeList->item(0);
    DOMElement* defElement = static_cast<DOMElement*>(defNode);
    DOMNodeList* macroList = xmlDoc->getElementsByTagName(XMLString::transcode("macros"));
    Teuchos::ParameterList tmPL;
    Teuchos::ParameterList cmPL;
    //loop over children
    DOMNodeList* children = macroList->item(0)->getChildNodes();
    for (int i=0; i<children->getLength(); i++) {
      DOMNode* currentNode = children->item(i) ;
      if (DOMNode::ELEMENT_NODE == currentNode->getNodeType()) {
	char* tagname = XMLString::transcode(currentNode->getNodeName());
	if (strcmp(tagname,"time_macro")==0) {
          Teuchos::ParameterList tm_parameter;
          attrMap = currentNode->getAttributes();
          nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	  if (nodeAttr) {
            textContent = XMLString::transcode(nodeAttr->getNodeValue());
	  } else {
	    Errors::Message msg;
	    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	    msg << "No attribute name found for time_macro. \n" ;
	    msg << "  Please correct and try again \n" ;
	    Exceptions::amanzi_throw(msg);
	  }

	  // deal differently if "times" or "start-inter-stop"
          DOMNodeList* childList = currentNode->getChildNodes();
	  bool isTime = false;
          for (int j=0; j<childList->getLength(); j++) {
            DOMNode* timeNode = childList->item(j) ;
            if (DOMNode::ELEMENT_NODE == timeNode->getNodeType()) {
	      if (strcmp(XMLString::transcode(timeNode->getNodeName()),"time")==0)
		      isTime = true;
	    }
	  }
	  if ( isTime ) {
            Teuchos::Array<double> times;
            for (int j=0; j<childList->getLength(); j++) {
              DOMNode* timeNode = childList->item(j) ;
              if (DOMNode::ELEMENT_NODE == timeNode->getNodeType()) {
	        char* nodeTxt = XMLString::transcode(timeNode->getTextContent());
	        times.append(get_time_value(nodeTxt,def_list));
	        XMLString::release(&nodeTxt);
	      }
	    }
	    tm_parameter.set<Teuchos::Array<double> >("Values", times);
	  } else {
            DOMElement* curElement = static_cast<DOMElement*>(currentNode);
            DOMNodeList* curList = curElement->getElementsByTagName(XMLString::transcode("start"));
            tmpNode = curList->item(0);
	    char* nodeTxt = XMLString::transcode(tmpNode->getTextContent());
            Teuchos::Array<double> sps;
	    sps.append(get_time_value(nodeTxt,def_list));
	    XMLString::release(&nodeTxt);
            curList = curElement->getElementsByTagName(XMLString::transcode("timestep_interval"));
	    if (curList->getLength() >0) {
              tmpNode = curList->item(0);
	      nodeTxt = XMLString::transcode(tmpNode->getTextContent());
	      sps.append(get_time_value(nodeTxt,def_list));
	      XMLString::release(&nodeTxt);
              curList = curElement->getElementsByTagName(XMLString::transcode("stop"));
	      if (curList->getLength() >0) {
                tmpNode = curList->item(0);
	        nodeTxt = XMLString::transcode(tmpNode->getTextContent());
	        sps.append(get_time_value(nodeTxt,def_list));
	        XMLString::release(&nodeTxt);
	      } else {
	        sps.append(-1.0);
	      }
	      tm_parameter.set<Teuchos::Array<double> >("Start_Period_Stop", sps);
	    } else {
	      tm_parameter.set<Teuchos::Array<double> >("Values", sps);
	    }
	  }
	  tmPL.sublist(textContent) = tm_parameter;
	  XMLString::release(&textContent);
	} else if (strcmp(tagname,"cycle_macro")==0) {
          Teuchos::ParameterList cm_parameter;
          attrMap = currentNode->getAttributes();
          nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	  if (nodeAttr) {
            textContent = XMLString::transcode(nodeAttr->getNodeValue());
	  } else {
	    Errors::Message msg;
	    msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing definitions - " ;
	    msg << "No attribute name found for cycle_macro. \n" ;
	    msg << "  Please correct and try again \n" ;
	    Exceptions::amanzi_throw(msg);
	  }

          DOMElement* curElement = static_cast<DOMElement*>(currentNode);
          DOMNodeList* curList = curElement->getElementsByTagName(XMLString::transcode("start"));
          tmpNode = curList->item(0);
	  char* nodeTxt = XMLString::transcode(tmpNode->getTextContent());
          Teuchos::Array<int> sps;
	  sps.append(get_int_constant(nodeTxt,def_list));
	  XMLString::release(&nodeTxt);
          curList = curElement->getElementsByTagName(XMLString::transcode("timestep_interval"));
	  if (curList->getLength() >0) {
            tmpNode = curList->item(0);
	    nodeTxt = XMLString::transcode(tmpNode->getTextContent());
	    sps.append(get_int_constant(nodeTxt,def_list));
	    XMLString::release(&nodeTxt);
            curList = curElement->getElementsByTagName(XMLString::transcode("stop"));
	    if (curList->getLength() >0) {
              tmpNode = curList->item(0);
	      nodeTxt = XMLString::transcode(tmpNode->getTextContent());
	      sps.append(get_int_constant(nodeTxt,def_list));
	      XMLString::release(&nodeTxt);
	    } else {
	      sps.append(-1.0);
	    }
	    cm_parameter.set<Teuchos::Array<int> >("Start_Period_Stop", sps);
	  } else {
	    cm_parameter.set<Teuchos::Array<int> >("Values", sps);
	  }
	  cmPL.sublist(textContent) = cm_parameter;
	  XMLString::release(&textContent);
	}
      }
    }
    list.sublist("Time Macros") = tmPL;
    list.sublist("Cycle Macros") = cmPL;
  }

  // get output node - this node must exist ONCE
  nodeList = xmlDoc->getElementsByTagName(XMLString::transcode("output"));
  DOMNode* outNode = nodeList->item(0);
  DOMElement* outElement = static_cast<DOMElement*>(outNode);
  if (DOMNode::ELEMENT_NODE == outNode->getNodeType()) {
    DOMNodeList* outChildList = outNode->getChildNodes();
    for (int m=0; m<outChildList->getLength(); m++) {
      DOMNode* curoutNode = outChildList->item(m) ;
      if (DOMNode::ELEMENT_NODE == curoutNode->getNodeType()) {
        char* outName = XMLString::transcode(curoutNode->getNodeName());
        if (strcmp(outName,"vis")==0) {

          // get list of vis - this node MAY exist ONCE
          DOMNodeList* childList = curoutNode->getChildNodes();
          Teuchos::ParameterList visPL;
          for (int j=0; j<childList->getLength(); j++) {
            DOMNode* curKid = childList->item(j) ;
            textContent  = XMLString::transcode(curKid->getNodeName());
            if (strcmp(textContent,"base_filename")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              visPL.set<std::string>("File Name Base",textContent2);
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"num_digits")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              visPL.set<int>("File Name Digits",get_int_constant(textContent2,def_list));
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"time_macros")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
	      Teuchos::Array<std::string> macro = make_regions_list(textContent2);
              visPL.set<Teuchos::Array<std::string> >("Time Macros",macro);
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"cycle_macros")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
	      Teuchos::Array<std::string> macro;
              macro.append(textContent2);
              visPL.set<Teuchos::Array<std::string> >("Cycle Macros",macro);
              //visPL.set<std::string>("Cycle Macros",textContent2);
              XMLString::release(&textContent2);
	    }
            XMLString::release(&textContent);
          }
          list.sublist("Visualization Data") = visPL;

	} else if (strcmp(outName,"checkpoint")==0) {
          // get list of checkpoint - this node MAY exist ONCE
          Teuchos::ParameterList chkPL;
          DOMNodeList* childList = curoutNode->getChildNodes();
          for (int j=0; j<childList->getLength(); j++) {
            DOMNode* curKid = childList->item(j) ;
            textContent  = XMLString::transcode(curKid->getNodeName());
            if (strcmp(textContent,"base_filename")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              chkPL.set<std::string>("File Name Base",textContent2);
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"num_digits")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              chkPL.set<int>("File Name Digits",get_int_constant(textContent2,def_list));
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"cycle_macro")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
	      Teuchos::Array<std::string> macro;
              macro.append(textContent2);
              //chkPL.set<Teuchos::Array<std::string> >("Cycle Macros",macro);
              chkPL.set<std::string >("Cycle Macro",textContent2);
              XMLString::release(&textContent2);
	    }
            XMLString::release(&textContent);
          }
          list.sublist("Checkpoint Data") = chkPL;
	} else if (strcmp(outName,"walkabout")==0) {
          // get list of walkabout - this node MAY exist ONCE
          Teuchos::ParameterList chkPL;
          DOMNodeList* childList = curoutNode->getChildNodes();
          for (int j=0; j<childList->getLength(); j++) {
            DOMNode* curKid = childList->item(j) ;
            textContent  = XMLString::transcode(curKid->getNodeName());
            if (strcmp(textContent,"base_filename")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              chkPL.set<std::string>("File Name Base",textContent2);
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"num_digits")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
              chkPL.set<int>("File Name Digits",get_int_constant(textContent2,def_list));
              XMLString::release(&textContent2);
	    } else if (strcmp(textContent,"cycle_macro")==0) {
	      textContent2 = XMLString::transcode(curKid->getTextContent());
	      Teuchos::Array<std::string> macro;
              macro.append(textContent2);
              //chkPL.set<Teuchos::Array<std::string> >("Cycle Macros",macro);
              chkPL.set<std::string >("Cycle Macro",textContent2);
              XMLString::release(&textContent2);
	    }
            XMLString::release(&textContent);
          }
          list.sublist("Walkabout Data") = chkPL;
	} else if (strcmp(outName,"observations")==0) {

          Teuchos::ParameterList obsPL;
          DOMNodeList* OBList = curoutNode->getChildNodes();
          for (int i=0; i<OBList->getLength(); i++) {
            DOMNode* curNode = OBList->item(i) ;
            if (DOMNode::ELEMENT_NODE == curNode->getNodeType()) {
              textContent  = XMLString::transcode(curNode->getNodeName());
              if (strcmp(textContent,"filename")==0) {
	        textContent2 = XMLString::transcode(curNode->getTextContent());
                obsPL.set<std::string>("Observation Output Filename",textContent2);
	        XMLString::release(&textContent2);
              } else if (strcmp(textContent,"liquid_phase")==0) {
                DOMNamedNodeMap* attrMap = curNode->getAttributes();
                DOMNode* nodeAttr = attrMap->getNamedItem(XMLString::transcode("name"));
	        char* phaseName;
		if (nodeAttr) {
                  phaseName = XMLString::transcode(nodeAttr->getNodeValue());
	        } else {
	          Errors::Message msg;
	          msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing observations - " ;
	          msg << "No attribute name found for liquid_phase. \n" ;
	          msg << "  Please correct and try again \n" ;
	          Exceptions::amanzi_throw(msg);
	        }

	        // loop over observations
	        DOMNodeList* childList = curNode->getChildNodes();
                for (int j=0; j<childList->getLength(); j++) {
	          Teuchos::ParameterList obPL;
                  DOMNode* curObs = childList->item(j) ;
                  if (DOMNode::ELEMENT_NODE == curObs->getNodeType()) {
                    char* obsType = XMLString::transcode(curObs->getNodeName());
                    if (strcmp(obsType,"aqueous_pressure")==0) {
	              obPL.set<std::string>("Variable","Aqueous pressure");
	            } else if (strcmp(obsType,"integrated_mass")==0) {
	              // TODO: EIB can't find matching version
	              //obPL.set<std::string>("Variable","Aqueous pressure");
                    } else if (strcmp(obsType,"volumetric_water_content")==0) {
	              obPL.set<std::string>("Variable","Volumetric water content");
                    } else if (strcmp(obsType,"gravimetric_water_content")==0) {
	              obPL.set<std::string>("Variable","Gravimetric water content");
                    } else if (strcmp(obsType,"x_aqueous_volumetric_flux")==0) {
	              // TODO: EIB needs double checking
	              obPL.set<std::string>("Variable","X-Aqueous volumetric flux");
                    } else if (strcmp(obsType,"y_aqueous_volumetric_flux")==0) {
	              // TODO: EIB needs double checking
	              obPL.set<std::string>("Variable","Y-Aqueous volumetric flux");
                    } else if (strcmp(obsType,"z_aqueous_volumetric_flux")==0) {
	              // TODO: EIB needs double checking
	              obPL.set<std::string>("Variable","Z-Aqueous volumetric flux");
                    } else if (strcmp(obsType,"material_id")==0) {
	              obPL.set<std::string>("Variable","MaterialID");
                    } else if (strcmp(obsType,"hydraulic_head")==0) {
	              obPL.set<std::string>("Variable","Hydraulic Head");
                    } else if (strcmp(obsType,"aqueous_mass_flow_rate")==0) {
	              obPL.set<std::string>("Variable","Aqueous mass flow rate");
                    } else if (strcmp(obsType,"aqueous_volumetric_flow_rate")==0) {
	              obPL.set<std::string>("Variable","Aqueous volumetric flow rate");
		    } else if (strcmp(obsType,"aqueous_saturation")==0) {
		      obPL.set<std::string>("Variable","Aqueous saturation");
                    } else if (strcmp(obsType,"aqueous_conc")==0) {
	              // get solute name
                      DOMNamedNodeMap* attrMap = curObs->getAttributes();
                      DOMNode* nodeAttr = attrMap->getNamedItem(XMLString::transcode("solute"));
	              char* soluteName ;
		      if (nodeAttr) {
                        soluteName = XMLString::transcode(nodeAttr->getNodeValue());
	              } else {
	                Errors::Message msg;
	                msg << "Amanzi::InputTranslator: ERROR - An error occurred during parsing observations - " ;
	                msg << "No attribute solute found for aqueous_conc. \n" ;
	                msg << "  Please correct and try again \n" ;
	                Exceptions::amanzi_throw(msg);
	              }

	              std::stringstream name;
	              name<< soluteName << " Aqueous concentration";
	              obPL.set<std::string>("Variable",name.str());
                    } else if (strcmp(obsType,"drawdown")==0) {
	              obPL.set<std::string>("Variable","Drawdown");
	            }
	            DOMNodeList* kidList = curObs->getChildNodes();
                    for (int k=0; k<kidList->getLength(); k++) {
                      DOMNode* curElem = kidList->item(k) ;
                      if (DOMNode::ELEMENT_NODE == curElem->getNodeType()) {
                        char* Elem =  XMLString::transcode(curElem->getNodeName());
                        char* Value =  XMLString::transcode(curElem->getTextContent());
		        if (strcmp(Elem,"assigned_regions")==0) {
		          //TODO: EIB - really a note, REGION != ASSIGNED REGIONS, this isn't consistent!!!
		          /*
	                  Teuchos::Array<std::string> regs;
	                  char* char_array;
	                  char_array = strtok(Value,",");
	                  while(char_array!=NULL){
	                    regs.append(char_array);
	                    char_array = strtok(NULL,",");
	                  }
                          obPL.set<Teuchos::Array<std::string> >("Region",regs);
		          */
                          obPL.set<std::string>("Region",Value);
		        } else if (strcmp(Elem,"functional")==0) {
	                  if (strcmp(Value,"point")==0) {
	                    obPL.set<std::string>("Functional","Observation Data: Point");
	                  } else if (strcmp(Value,"integral")==0) {
	                    obPL.set<std::string>("Functional","Observation Data: Integral");
	                  } else if (strcmp(Value,"mean")==0) {
	                    obPL.set<std::string>("Functional","Observation Data: Mean");
	                  }
		        } else if (strcmp(Elem,"time_macro")==0) {
	                  obPL.set<std::string>("Time Macro",Value);
		        }
                        XMLString::release(&Elem);
                        XMLString::release(&Value);
	              }
	            }
	            std::stringstream listName;
	            listName << "observation-"<<j+1<<":"<<phaseName;
	            obsPL.sublist(listName.str()) = obPL;
	          }
	        }
	        XMLString::release(&phaseName);
              }
              XMLString::release(&textContent);
              list.sublist("Observation Data") = obsPL;
            }
          }
	}
      }
    }
  }

  return list;
  
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

//TODO: EIB - get default time unit from units, convert plain time values if not seconds.

double get_time_value(std::string time_value, Teuchos::ParameterList def_list)
{

  double time;

  // Check if time_value is: listed in constants
  if (def_list.sublist("constants").sublist("constants").isSublist(time_value)) {
    time = def_list.sublist("constants").sublist("constants").sublist(time_value).get<double>("value");

  // Check if time_value is: listed in time_constants
  } else if (def_list.sublist("constants").sublist("time_constants").isSublist(time_value)) {
    time = def_list.sublist("constants").sublist("time_constants").sublist(time_value).get<double>("value");

  // Otherwise, it must be a string or typedef_labeled_time (0000.00,y)
  } else {
    char* tmp = strcpy(new char[time_value.size() + 1], time_value.c_str());
    char* char_array = strtok(tmp,";, ");
    time = atof(char_array);
    char_array = strtok(NULL,";, ");
    if (char_array!=NULL) {
      if (strcmp(char_array,"y")==0) { time = time*365.25*24.0*60.0*60.0; }
      else if (strcmp(char_array,"d")==0) { time = time*24.0*60.0*60.0; }
      else if (strcmp(char_array,"h")==0) { time = time*60.0*60.0; }
    }
    delete[] tmp;
  }

  return time;
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */


double get_double_constant(std::string pos_name, Teuchos::ParameterList def_list)
{

  double value;

  // Check if pos_name is: listed in constants
  if (def_list.sublist("constants").sublist("constants").isSublist(pos_name)) {
    value = def_list.sublist("constants").sublist("constants").sublist(pos_name).get<double>("value");

  // Check if pos_name is: listed in time_constants
  } else if (def_list.sublist("constants").sublist("numerical_constant").isSublist(pos_name)) {
    value = def_list.sublist("constants").sublist("numerical_constant").sublist(pos_name).get<double>("value");

  // Otherwise, we must assume it's already a value
  } else {
    value = atof(pos_name.c_str());
  }
  
  return value;
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */


int get_int_constant(std::string pos_name, Teuchos::ParameterList def_list)
{

  int value;

  // Check if pos_name is: listed in constants
  if (def_list.sublist("constants").sublist("constants").isSublist(pos_name)) {
    value = def_list.sublist("constants").sublist("constants").sublist(pos_name).get<int>("value");

  // Check if pos_name is: listed in time_constants
  } else if (def_list.sublist("constants").sublist("numerical_constant").isSublist(pos_name)) {
    value = def_list.sublist("constants").sublist("numerical_constant").sublist(pos_name).get<int>("value");

  // Otherwise, we must assume it's already a value
  } else {
    value = atoi(pos_name.c_str());
  }
  
  return value;
}


/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

Teuchos::Array<std::string> make_regions_list(char* char_array)
{
  Teuchos::Array<std::string> regs;
  char* tmp;
  tmp = strtok(char_array,",");
  while(tmp!=NULL){
    std::string str(tmp);
    boost::algorithm::trim(str);
    regs.append(str);
    tmp = strtok(NULL,",");
  }

  return regs;
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

bool compare_region_names(Teuchos::Array<std::string> regions, Teuchos::ParameterList def_list)
{
  int cnt;
  cnt = 0;
  bool status=true;
  for (int i = 0; i < regions.size(); i++) { 
    if (def_list.sublist("regions").isParameter(regions[i].c_str())) {
      cnt++;
    } else {
      std::cout << "Amanzi::InputTranslator: ERROR - region "<< regions[i] << " NOT in known regions!" << std::endl;
      return false;
    }
  }

  return status;
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

Teuchos::Array<double> make_coordinates(char* char_array, Teuchos::ParameterList def_list)
{
  Teuchos::Array<double> coords;
  char* tmp;
  tmp = strtok(char_array,"(,");
  while(tmp!=NULL){
    std::string str(tmp);
    boost::algorithm::trim(str);
    coords.append(get_double_constant(str, def_list));
    tmp = strtok(NULL,",");
  }

  return coords;
}

/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

Teuchos::ParameterList make_chemistry(Teuchos::ParameterList def_list)
{
    Teuchos::ParameterList chemistryPL;
    Teuchos::ParameterList bgdPL;

    // build bgd filename
    std::string bgdfilename;
    if (def_list.isParameter("xmlfilename") ) {
        bgdfilename = def_list.get<std::string>("xmlfilename") ;
        std::string new_extension(".bgd");
        size_t pos = bgdfilename.find(".xml");
        bgdfilename.replace(pos, (size_t)4, new_extension, (size_t)0, (size_t)4);
    } else {
        // defaulting to hardcoded name
        bgdfilename = "isotherms.bgd" ;
    }

    bgdPL.set<std::string>("Format","simple");
    bgdPL.set<std::string>("File",bgdfilename);
    chemistryPL.sublist("Thermodynamic Database") = bgdPL;
    chemistryPL.set<std::string>("Activity Model","unit");
    Teuchos::Array<std::string> verb;
    if (def_list.sublist("simulation").isParameter("verbosity")) {
        std::string verbosity = def_list.sublist("simulation").get<std::string>("verbosity") ;
        if (verbosity == "extreme") {
	    verb.append("error");
	    chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
        } else if (verbosity == "high") {
	    verb.append("warning");
	    chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
	} else if (verbosity == "medium") {
	    verb.append("verbose");
	    chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
	} else if (verbosity == "low") {
	    verb.append("terse");
	    chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
	} else {
	    verb.append("silent");
	    chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
	}
    } else {
	verb.append("silent");
	chemistryPL.set<Teuchos::Array<std::string> >("Verbosity",verb);
    }

    // fill in default values
    //chemistryPL.set<double>("Tolerance",1e-12);
    //chemistryPL.set<int>("Maximum Newton Iterations",200);
    //chemistryPL.set<double>("Max Time Step (s)",9e9);
    
    return chemistryPL;
}


/* 
 ******************************************************************
 * Empty
 ******************************************************************
 */

void write_BDG_file(Teuchos::ParameterList sorption_list, Teuchos::ParameterList def_list)
{

  std::ofstream bgd_file;
  std::stringstream species_string;
  std::stringstream isotherms_string;

  // build streams
  for (Teuchos::ParameterList::ConstIterator i = sorption_list.begin(); i != sorption_list.end(); i++) {
      Teuchos::ParameterList& tmpList = sorption_list.sublist(sorption_list.name(i)) ;
      species_string << sorption_list.name(i) << " ;   0.00 ;   0.00 ;   1.00 \n";
      if ( tmpList.isParameter("Langmuir b") ) {
          isotherms_string << sorption_list.name(i) << " ; langmuir ; " << tmpList.get<double>("Kd")<< " " <<tmpList.get<double>("Langmuir b") << std::endl;
      } else if ( tmpList.isParameter("Freundlich n") ) {
          isotherms_string << sorption_list.name(i) << " ; freundlich ; " << tmpList.get<double>("Kd")<< " " <<tmpList.get<double>("Freundlich n") << std::endl;
      } else {
          isotherms_string << sorption_list.name(i) << " ; linear ; " << tmpList.get<double>("Kd")<< std::endl;
      }
  }
  
  // build bgd filename
  std::string bgdfilename;
  if (def_list.isParameter("xmlfilename") ) {
      bgdfilename = def_list.get<std::string>("xmlfilename") ;
      std::string new_extension(".bgd");
      size_t pos = bgdfilename.find(".xml");
      bgdfilename.replace(pos, (size_t)4, new_extension, (size_t)0, (size_t)4);
  } else {
      // defaulting to hardcoded name
      bgdfilename = "isotherms.bgd" ;
  }

  // open output bgd file
  bgd_file.open(bgdfilename.c_str());

  // <Primary Species
  bgd_file << "<Primary Species\n";
  bgd_file << species_string.str();

  //<Isotherms
  bgd_file << "<Isotherms\n" ;
  bgd_file << "# Note, these values will be overwritten by the xml file\n" ;
  bgd_file << isotherms_string.str();

  // close output bdg file
  bgd_file.close();
}


} // end namespace AmanziNewInput
} // end namespace Amanzi
