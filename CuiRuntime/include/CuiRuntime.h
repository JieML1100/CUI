#pragma once

#include <Resource.h>

/**
 * Public umbrella header for CUI's dynamic design-document runtime.
 *
 * Applications normally need RuntimeDocument, RuntimeEventHandlerRegistry,
 * or RuntimeDocumentSession, plus the serializer when they load .cui.xml files. The lower-level
 * materializer remains available for hosts that provide a custom factory.
 */
#include "../../CuiDesigner/DesignerModel/RuntimeDocument.h"
#include "../../CuiDesigner/DesignerModel/RuntimeEventHandlerRegistry.h"
#include "../../CuiDesigner/DesignerModel/RuntimeDocumentFileWatcher.h"
#include "../../CuiDesigner/DesignerModel/RuntimeDocumentSession.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentFileFormat.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentEventIndex.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentSerializer.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentMaterializer.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentSerializer.h"
