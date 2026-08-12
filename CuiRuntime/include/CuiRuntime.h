#pragma once

#include <Resource.h>
#include "../../CUI/include/ComponentBehavior.h"
#include "../../CUI/include/NativeSurface.h"
#include "../../CUI/include/BindingList.h"
#include "../../CUI/include/CollectionViewSource.h"
#include "../../CUI/include/ContentControl.h"
#include "../../CUI/include/ControlTemplate.h"
#include "../../CUI/include/ItemContainer.h"
#include "../../CUI/include/HeaderedContentControl.h"
#include "../../CUI/include/ContentPresenter.h"
#include "../../CUI/include/Decorator.h"
#include "../../CUI/include/Border.h"
#include "../../CUI/include/GroupStyle.h"
#include "../../CUI/include/ItemTemplate.h"
#include "../../CUI/include/ItemsPanelTemplate.h"
#include "../../CUI/include/ItemsControl.h"
#include "../../CUI/include/ItemsPresenter.h"
#include "../../CUI/include/Selector.h"
#include "../../CUI/include/ListBox.h"
#include "../../CUI/include/DataGrid.h"

/**
 * Public umbrella header for CUI's dynamic design-document runtime.
 *
 * Applications normally need RuntimeDocument, RuntimeEventHandlerRegistry,
 * or RuntimeDocumentSession when they load .cui.xaml files. The lower-level
 * materializer remains available for hosts that provide a custom factory.
 */
#include "../../CuiDesigner/DesignerModel/RuntimeDocument.h"
#include "../../CuiDesigner/DesignerModel/RuntimeEventHandlerRegistry.h"
#include "../../CuiDesigner/DesignerModel/RuntimeDocumentFileWatcher.h"
#include "../../CuiDesigner/DesignerModel/RuntimeDocumentSession.h"
#include "../../CuiDesigner/DesignerModel/DesignDocumentEventIndex.h"
#include "XamlRuntimeSchema.h"
#include "XamlDocumentCompiler.h"
#include "XamlFrameworkTheme.h"
#include "XamlObjectMaterializer.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentParser.h"
#include "../../CuiDesigner/DesignerModel/XamlDocumentSerializer.h"
