//														
//		     Copyright (C)  2017
//     University Corporation for Atmospheric Research
//		     All Rights Reserved					
//							
//////////////////////////////////////////////////////////////////////////
//
//	File:		RenderEventRouter.cpp
//
//	Author:		John Clyne
//			National Center for Atmospheric Research
//			PO 3000, Boulder, Colorado
//
//	Date:		January 2017
//
//	Description:	Implements the (pure virtual) RenderEventRouter class.
//		This class supports routing messages from the gui to the params
//
#ifdef WIN32
//Annoying unreferenced formal parameter warning
#pragma warning( disable : 4100 )
#endif


#include <QFileDialog>
#include <vapor/GetAppPath.h>
#include <vapor/DataMgrUtils.h>
#include "ErrorReporter.h"
#include "MappingFrame.h"
#include "RenderEventRouter.h"

using namespace VAPoR;


RenderParams *RenderEventRouter::GetActiveParams() const {
    assert(! _instName.empty());

    ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

	string winName, dataSetName, paramsType;
	bool status = paramsMgr->RenderParamsLookup(
		_instName, winName, dataSetName, paramsType
	);
	assert(status);

	string renderType = RendererFactory::Instance()->
        GetRenderClassFromParamsClass(paramsType);

    return(_controlExec->GetRenderParams(
		winName, dataSetName, renderType, _instName
	));
}

DataMgr *RenderEventRouter::GetActiveDataMgr() const {
    assert(! _instName.empty());

    ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

	string winName, dataSetName, paramsType;

	bool status = paramsMgr->RenderParamsLookup(
		_instName, winName, dataSetName, paramsType
	);
	assert(status);

	DataStatus *dataStatus = _controlExec->GetDataStatus();
	DataMgr *dataMgr = dataStatus->GetDataMgr(dataSetName);
	assert(dataMgr);

	return(dataMgr);
}

string RenderEventRouter::GetSmallIconImagePath() const {
    string imageName = _getSmallIconImagePath();
    if (imageName.empty()) return (imageName);

	vector <string> path;
    path.push_back("images");
    path.push_back(imageName);
    return (GetAppPath("VAPOR", "share", path));
}

string RenderEventRouter::GetIconImagePath() const {
    string imageName = _getIconImagePath();
    if (imageName.empty()) return (imageName);

	vector <string> path;
    path.push_back("images");
    path.push_back(imageName);
    return (GetAppPath("VAPOR", "share", path));
}



void RenderEventRouter::setEditorDirty() {
	RenderParams *rParams = GetActiveParams();
	
	MappingFrame *mp = getMappingFrame();
	if (!mp) return;

	mp->Update(GetActiveDataMgr(),
		_controlExec->GetParamsMgr(),
		rParams);
}


#ifdef	VAPOR3_0_0_ALPHA
float RenderEventRouter::CalcCurrentValue(const double point[3] ){
	RenderParams *rParams = GetActiveParams();
	
	if (!rParams->IsEnabled()) return 0.f;
	
	size_t timeStep = GetCurrentTimeStep();

#ifdef	VAPOR3_0_0_ALPHA
	if (rParams->doBypass(timeStep)) return _OUT_OF_BOUNDS;
#endif
	
	vector <double> minExts, maxExts;
	for (int i = 0; i<3; i++){
		minExts.push_back(point[i]);
		maxExts.push_back(point[i]);
	}

	string varname = rParams->GetVariableName();
	if (varname.empty()) return(0.0);

	vector<string> varnames;
	varnames.push_back(varname);

	StructuredGrid* grid;
	//Get the data dimensions (at current resolution):
	
	int numRefinements = rParams->GetRefinementLevel();
	int lod = rParams->GetCompressionLevel();

	DataStatus *dataStatus = _controlExec->getDataStatus();
	int rc = dataStatus->getGrids(
		timeStep, varnames, minExts, maxExts, 
		&numRefinements, &lod, &grid
	);
	
#ifdef	VAPOR3_0_0_ALPHA
	if (rc<0) return _OUT_OF_BOUNDS;
#endif
	float varVal = (grid)->GetValue(point[0],point[1],point[2]);
	
	delete grid;
	return varVal;
}
#endif

void RenderEventRouter::updateTab(){
	if (_instName.empty()) return;

	RenderParams *rParams = GetActiveParams();

	//If the Params is not valid do not proceed.
	if (!rParams) return;

	DataMgr *dataMgr = GetActiveDataMgr();
	if (!dataMgr) return;


	EventRouter::updateTab();
}



RenderEventRouter *RenderEventRouterFactory::CreateInstance(
	string className, QWidget * parent, VAPoR::ControlExec *ce
) {
    RenderEventRouter * instance = NULL;

    // find className in the registry and call factory method.
	//
    auto it = _factoryFunctionRegistry.find(className);
    if(it != _factoryFunctionRegistry.end())
        instance = it->second(parent, ce);

    if(instance != NULL)
        return instance;
    else
        return NULL;
}


vector <string> RenderEventRouterFactory::GetFactoryNames() const {
	vector <string> names;
	map<string, function<RenderEventRouter * (QWidget*, VAPoR::ControlExec *)>>::const_iterator itr;

	for (
		itr = _factoryFunctionRegistry.begin();
		itr!=_factoryFunctionRegistry.end();
		++itr
	) {
		names.push_back(itr->first);
	}
	return(names);
}
