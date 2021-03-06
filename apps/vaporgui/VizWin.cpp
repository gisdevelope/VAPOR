//************************************************************************
//									*
//			 Copyright (C)  2013				*
//	 University Corporation for Atmospheric Research			*
//			 All Rights Reserved				*
//									*
//************************************************************************/
//	File:		VizWin.cpp
//
//	Author:		Alan Norton
//			National Center for Atmospheric Research
//			PO 3000, Boulder, Colorado
//
//	Date:		October 2013
//
//	Description:	Implements the VizWin class
//		This is the QGLWidget that performs OpenGL rendering (using associated 
//		Visualizer).
//
//		Supports mouse event reporting.
//
#include <vapor/glutil.h>	// Must be included first!!!
#include <cassert>
#include <QResizeEvent>
#include <QFocusEvent>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QIcon>
#include <vapor/ControlExecutive.h>
#include <vapor/ViewpointParams.h>
#include <vapor/Viewpoint.h>
#include <vapor/debug.h>
#include <vapor/ImageParams.h>
#include "TrackBall.h"
#include "GUIStateParams.h"
#include "MouseModeParams.h"
#include "AnimationParams.h"
#include "qdatetime.h"
#include "ErrorReporter.h"
#include "images/vapor-icon-32.xpm"
#include "VizWin.h"
#include "Core3_2_context.h"
#include <glm/gtc/type_ptr.hpp>
#include "vapor/GLManager.h"
#include "vapor/LegacyGL.h"
#include "vapor/FileUtils.h"

using namespace VAPoR;


/*
 *  Constructs a VizWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 */
VizWin::VizWin(
    const QGLFormat &format,
	QWidget* parent, const QString& name, 
	string winName, ControlExec *ce, Trackball *trackBall
) : QGLWidget( new Core3_2_context(format), parent )
{
	_trackBall = trackBall;
	
	setAttribute(Qt::WA_DeleteOnClose);
	_winName = winName;
	setWindowIcon(QPixmap(vapor_icon___));
	_controlExec = ce;
    
    // TODO GL
    _glManager = new GLManager;
    vector <string> paths; paths.push_back("shaders");
    string shaderPath = GetAppPath("VAPOR", "share", paths);
    paths.clear(); paths.push_back("fonts");
    string fontPath = GetAppPath("VAPOR", "share", paths);
    _glManager->shaderManager->SetResourceDirectory(shaderPath); // TODO GL
    _glManager->fontManager->SetResourceDirectory(fontPath); // TODO GL

	setAutoBufferSwap(false);
	_mouseClicked = false;
	_buttonNum = 0;
	_navigateFlag = false;
	_manipFlag = false;
	_openGLInitFlag = false;

	setMouseTracking(false);	// Only track mouse when button clicked/held

	_manip = new TranslateStretchManip(_glManager);
	bool initialize = true;
	updateManip(initialize);
}

/*
 *  Destroys the object and frees any allocated resources
 */
VizWin::~VizWin()
{
    delete _glManager;
}

// void VizWin::makeCurrent()
// {
// 	QGLWidget::makeCurrent();
// }

void VizWin::closeEvent(QCloseEvent* e){

	// Remove the visualizer. This must be done inside of VizWin so that
	// the OpenGL context can be made current because removing a visualizer
	// triggers OpenGL calls in the renderer destructors
	//
	makeCurrent();

	emit Closing(_winName);

	QWidget::closeEvent(e);
}

/******************************************************
 * React when focus is on window:
 ******************************************************/
void VizWin::focusInEvent(QFocusEvent* e){
	//Test for hidden here, since a vanishing window can get this event.
	if (e->gotFocus() && !isHidden()){

		emit HasFocus(_winName);
	}
}

// First project all 8 box corners to the center line of the camera 
// view, finding the furthest and nearest projection in front of the 
// camera.  The furthest distance is used as the far distance.
// If some point projects behind the camera, then either the 
// camera is inside the box, or a corner of the
// box is behind the camera.  This calculation is always performed in 
// local coordinates since a translation won't affect
// the result
//
void VizWin::_getNearFarDist(
	const double  posVec[3], const double dirVec[3],
	double &boxNear,  double &boxFar
) const {

	//First check full box
	double wrk[3], cor[3], boxcor[3];
	double camPosBox[3],dvdir[3];
#ifdef WIN32
	double maxProj = -DBL_MAX;
	double minProj = DBL_MAX;
#else
	double maxProj = -std::numeric_limits<double>::max();
	double minProj = std::numeric_limits<double>::max();
#endif

	DataStatus *dataStatus = _controlExec->GetDataStatus();
	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

	AnimationParams* p = (AnimationParams *) paramsMgr->GetParams(
		AnimationParams::GetClassType()
	);
	size_t ts = p->GetCurrentTimestep();

	vector <double> minExts, maxExts;
	dataStatus->GetActiveExtents(
		paramsMgr, _winName, ts, minExts, maxExts
	);

	for (int i = 0; i<3; i++) camPosBox[i] = posVec[i];
	
	
	for (int i = 0;i<3; i++) dvdir[i] =  dirVec[i];
	vnormal(dvdir);
	
	//For each box corner, 
	//   convert to box coords, then project to line of view
	for (int i = 0; i<8; i++){
		for (int j = 0; j< 3; j++){
			cor[j] = ( (i>>j)&1) ? maxExts[j] : minExts[j];
		}
		for (int k=0;k<3;k++) boxcor[k] = cor[k];
		
		vsub(boxcor, camPosBox, wrk);
		
		float mdist = abs(vdot(wrk, dvdir));
		if (minProj > mdist) {
			minProj = mdist;
		}
		if (maxProj < mdist) {
			maxProj = mdist;
		}
	}

	if (maxProj < 1.e-10) maxProj = 1.e-10;
	if (minProj > 0.03*maxProj) minProj = 0.03*maxProj;

	// minProj will be < 0 if either the camera is in the box, or
	// if some of the region is behind the camera plane.  In that case, just
	// set the nearDist a reasonable multiple of the fardist
	//
	if (minProj <= 0.0) minProj = 0.0002*maxProj;
	boxFar = (float)maxProj;
	boxNear = (float)minProj;
	
	return;
}

void VizWin::_setUpProjMatrix() {

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);
    // _controlExec->visu

	double m[16];
	vParams->GetModelViewMatrix(m);

	double posvec[3], upvec[3], dirvec[3];
	bool status = vParams->ReconstructCamera(m, posvec, upvec, dirvec);
	if (! status) {
		MSG_ERR("Failed to get camera parameters");
		return;
	}

	double nearDist, farDist;
	_getNearFarDist(posvec, dirvec, nearDist, farDist);
	nearDist *= 0.25;
	farDist *= 4.0;

	size_t width, height;
	vParams->GetWindowSize(width, height);

    _glManager->matrixManager->MatrixModeProjection();
    _glManager->matrixManager->LoadIdentity();

	GLfloat w = (float) width / (float) height;

	double fov = vParams->GetFOV();
    _glManager->matrixManager->Perspective(fov, w, nearDist, farDist);
    // float s = 1000000;
    // _glManager->matrixManager->Ortho(-s, s, -s, s, nearDist, farDist);

	double pMatrix[16];
    _glManager->matrixManager->GetDoublev(MatrixManager::Mode::Projection, pMatrix);

	bool enabled = _controlExec->GetSaveStateEnabled();
	_controlExec->SetSaveStateEnabled(false);

	vParams->SetProjectionMatrix(pMatrix);

	_controlExec->SetSaveStateEnabled(enabled);

    _glManager->matrixManager->MatrixModeModelView();
}

void VizWin::_setUpModelViewMatrix() {

	makeCurrent();	// necessary?

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);

	double m[16];
	vParams->GetModelViewMatrix(m);
    _glManager->matrixManager->LoadMatrixd(m);

}

// React to a user-change in window size/position (or possibly max/min)
// Either the window is minimized, maximized, restored, or just resized.
//
void VizWin::resizeGL(int width, int height){

	if (! _openGLInitFlag || ! FrameBufferReady()) {
		return;
	}

	int rc = printOpenGLErrorMsg("GLVizWindowResizeEvent");
	if (rc < 0) {
		MSG_ERR("OpenGL error");
	}

	rc = _controlExec->ResizeViz(_winName, width, height);
	if (rc < 0) {
		MSG_ERR("OpenGL error");
	}

	glViewport( 0, 0, (GLint)width, (GLint)height );

	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT);
	swapBuffers();

	// Necessary?
	//
	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT);
	swapBuffers();

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);

	// Window size can't be undone, so disable state saving
	//
	bool enabled = _controlExec->GetSaveStateEnabled();
	_controlExec->SetSaveStateEnabled(false);
	vParams->SetWindowSize(width, height);
	_controlExec->SetSaveStateEnabled(enabled);
}

void VizWin::initializeGL(){

	printOpenGLErrorMsg("GLVizWindowInitializeEvent");
	int rc = _controlExec->InitializeViz(_winName, _glManager);
	if (rc<0) {
		MSG_ERR("Failure to initialize Visualizer");
	}
    _glManager->legacy->Initialize();
	printOpenGLErrorMsg("GLVizWindowInitializeEvent");

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);

	if (vParams != NULL) {
		bool enabled = _controlExec->GetSaveStateEnabled();
		_controlExec->SetSaveStateEnabled(false);
		vParams->SetWindowSize(width(), height());
		_controlExec->SetSaveStateEnabled(enabled);
	}

	_openGLInitFlag = true;
}

void VizWin::_mousePressEventManip(QMouseEvent* e) {

	std::vector<double> screenCoords = _getScreenCoords(e);

	_manipFlag = _manip->MouseEvent(
		_buttonNum, screenCoords, _strHandleMid
	);

}

void VizWin::_mousePressEventNavigate(QMouseEvent* e) {
	_navigateFlag = true;

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

    double m[16];
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);
    vParams->GetModelViewMatrix(m);

	double center[3];
	vParams->GetRotationCenter(center);

	double posvec[3], upvec[3], dirvec[3];
	bool status = vParams->ReconstructCamera(m, posvec, upvec, dirvec);
	assert(status);

	// Set trackball from current ViewpointParams matrix;
	//
	_trackBall->setFromFrame(posvec, dirvec, upvec, center, true);
	_trackBall->TrackballSetMatrix();	// needed?

	// Let trackball handle mouse events for navigation
	//
	_trackBall->MouseOnTrackball(
		0, _buttonNum, e->x(), e->y(), width(), height()
	);

	// Create a state saving group.
	// Only save camera parameters after user release mouse
	//
	paramsMgr->BeginSaveStateGroup("Navigate scene");
	emit StartNavigation(_winName);
}

// If the user presses the mouse on the active viz window,
// We record the position of the click.
//
void VizWin::mousePressEvent(QMouseEvent* e) {

	_buttonNum = 0;
	_mouseClicked = true;


	if ((e->buttons() & Qt::LeftButton) &&  (e->buttons() & Qt::RightButton))
		;//do nothing
	else if (e->button()== Qt::LeftButton) _buttonNum = 1;
	else if (e->button() == Qt::RightButton) _buttonNum = 3;
	else if (e->button() == Qt::MidButton) _buttonNum = 2;
	//If ctrl + left button is pressed, only respond in navigation mode
	if (
		(_buttonNum == 1) && 
		((e->modifiers() & (Qt::ControlModifier|Qt::MetaModifier)))
	) {
		_buttonNum = 0;
	}

	if (_buttonNum == 0) {
		_mouseClicked = true;	// mouse button is held
		return;
	}

	string modeName = _getCurrentMouseMode();

	if (modeName == MouseModeParams::GetRegionModeName()) {
		_mousePressEventManip(e);

		// Only manipulating if user managed to grab manipulator handle.
		// Otherwise we navigate
		//
		if (_manipFlag) {
			return;
		}
	}

	_mousePressEventNavigate(e);
}

void VizWin::_mouseReleaseEventManip(QMouseEvent*e) {

	if (! _manipFlag) return;

	std::vector<double> screenCoords = _getScreenCoords(e);
	(void) _manip->MouseEvent(
		_buttonNum, screenCoords, _strHandleMid, true
	);
	_setNewExtents();

	_manipFlag = false;
}

void VizWin::_mouseReleaseEventNavigate(QMouseEvent*e) {
	if (! _navigateFlag) return;

	_trackBall->MouseOnTrackball(
		2, _buttonNum, e->x(), e->y(), width(), height()
	);
	_trackBall->TrackballSetMatrix();

	const double *m = _trackBall->GetModelViewMatrix();

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

	// Set modelview matrix in ViewpointParams. The Visualizer 
	// will use download this to OpenGL
	//
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);
	vParams->SetModelViewMatrix(m);

	paramsMgr->EndSaveStateGroup();

	emit EndNavigation(_winName);

	_navigateFlag = false;
}

/*
 * If the user releases the mouse or moves it (with the left mouse down)
 * then we note the displacement
 */
void VizWin::mouseReleaseEvent(QMouseEvent*e){

	if (_buttonNum == 0) return;

	_mouseClicked = false;

	if (_manipFlag) {
		_mouseReleaseEventManip(e);
	}
	else if (_navigateFlag) {
		_mouseReleaseEventNavigate(e);
	}

	_buttonNum = 0;
}

void VizWin::_mouseMoveEventManip(QMouseEvent* e) {
	if (! _manipFlag) return;

	std::vector<double> screenCoords = _getScreenCoords(e);

	(void) _manip->MouseEvent(
		_buttonNum, screenCoords, _strHandleMid
	);
}

void VizWin::_mouseMoveEventNavigate(QMouseEvent* e) {
	if (! _navigateFlag) return;

	_trackBall->MouseOnTrackball(
		1, _buttonNum, e->x(), e->y(), width(), height()
	);

	_trackBall->TrackballSetMatrix();

	const double *m = _trackBall->GetModelViewMatrix();

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();

	// Set modelview matrix in ViewpointParams
	//
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);
	vParams->SetModelViewMatrix(m);
	Render(true);
}

std::vector<double> VizWin::_getScreenCoords(QMouseEvent* e) const{
	std::vector<double> screenCoords;
	screenCoords.push_back((double)e->x());
	screenCoords.push_back((double)(height() - e->y()));
	return screenCoords;
}

string VizWin::_getCurrentMouseMode() const {
	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	GUIStateParams *guiP = (GUIStateParams *) paramsMgr->GetParams(
		GUIStateParams::GetClassType()
	);
	MouseModeParams *p = guiP->GetMouseModeParams();
	string modeName = p->GetCurrentMouseMode();
	return modeName;
}

void VizWin::_setNewExtents() {
	std::vector<double> llc, urc;
	_manip->GetBox(llc,urc);
	VAPoR::RenderParams* rParams = _getRenderParams();
	if (rParams==NULL)
		return;
	VAPoR::Box* box = rParams->GetBox();
	std::vector<double> pllc, purc;
	box->GetExtents(pllc, purc);

	box->SetExtents(llc, urc);
}

/* 
 * When the mouse is moved, it can affect navigation, 
 * region position, light position, or probe position, depending 
 * on current mode.  The values associated with the window are 
 * changed whether or not the tabbed panel is visible.  
 *  It's important that coordinate changes eventually get recorded in the
 * viewpoint params panel.  This requires some work every time there is
 * mouse navigation.  Changes in the viewpoint params panel will notify
 * the viztab if it is active and change the values there.
 * Conversely, when values are changed in the viztab, the viewpoint
 * values are set in the VizWin class, provided they did not originally 
 * come from the mouse navigation.  Such a change forces a reinitialization
 * of the trackball and the new values will be used at the next rendering.
 * 
 */
void VizWin::mouseMoveEvent(QMouseEvent* e){

	if (_buttonNum == 0) return;

	if (_manipFlag) {
		_mouseMoveEventManip(e);
	} 
	else if (_navigateFlag) {
		_mouseMoveEventNavigate(e);
	}
	return;
}

void VizWin::setFocus(){
	QWidget::setFocus();
}

void VizWin::Render(bool fast) {

	// Need to call since we're not overriding QGLWidget::paintGL()
	//
	makeCurrent();

	if (! _openGLInitFlag || ! FrameBufferReady()) {
		return;
	}

	ParamsMgr *paramsMgr = _controlExec->GetParamsMgr();
	ViewpointParams* vParams = paramsMgr->GetViewpointParams(_winName);

	bool enabled = _controlExec->GetSaveStateEnabled();
	_controlExec->SetSaveStateEnabled(false);
	vParams->SetWindowSize(width(), height());
	_controlExec->SetSaveStateEnabled(enabled);

	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT);
    
	DataStatus *dataStatus = _controlExec->GetDataStatus();
	if (! dataStatus->GetDataMgrNames().size()) return;

	// Set up projection and modelview matrices
	//
    _glManager->matrixManager->MatrixModeProjection();
    _glManager->matrixManager->PushMatrix();
	_setUpProjMatrix();

    _glManager->matrixManager->MatrixModeModelView();
    _glManager->matrixManager->PushMatrix();
	_setUpModelViewMatrix();

	int rc = _controlExec->Paint(_winName, fast);
	if (rc < 0) {
		MSG_ERR("Paint failed");
	}

	if (_getCurrentMouseMode() == MouseModeParams::GetRegionModeName()) {
		updateManip();
	}
	
	swapBuffers();
    
	rc = printOpenGLErrorMsg("VizWindowPaintGL");
	if (rc < 0) {
		MSG_ERR("OpenGL error");
	}

	_glManager->matrixManager->MatrixModeProjection();
    _glManager->matrixManager->PopMatrix();
	_glManager->matrixManager->MatrixModeModelView();
    _glManager->matrixManager->PopMatrix();
}

VAPoR::RenderParams* VizWin::_getRenderParams() {
	string className;
	return _getRenderParams(className);
}

VAPoR::RenderParams* VizWin::_getRenderParams(string &className) {
	ParamsMgr* paramsMgr = _controlExec->GetParamsMgr();
	GUIStateParams *guiP = (GUIStateParams *) paramsMgr->GetParams(
		GUIStateParams::GetClassType()
	);

	string inst, winName, dataSetName;
	guiP->GetActiveRenderer(_winName, className, inst);

	bool exists = paramsMgr->RenderParamsLookup(
		inst, winName, dataSetName, className
	);

	if (!exists)
		return NULL;

//	VAPoR::RenderParams* rParams = _controlExec->GetRenderParams(
//		_winName, dataSetName, className, inst
//	);
	VAPoR::RenderParams* rParams = paramsMgr->GetRenderParams(
		_winName, dataSetName, className, inst
	);

	return rParams;
}

string VizWin::_getCurrentDataMgrName() const {
	ParamsMgr* paramsMgr = _controlExec->GetParamsMgr();
	GUIStateParams *guiP = (GUIStateParams *) paramsMgr->GetParams(
		GUIStateParams::GetClassType()
	);

	string inst, winName, className, dataSetName;
	guiP->GetActiveRenderer(_winName, className, inst);

	bool exists = paramsMgr->RenderParamsLookup(
		inst, winName, dataSetName, className
	);

	if (!exists)
		return "";

	return dataSetName;
}

void VizWin::_getUnionOfFieldVarExtents(
	RenderParams* rParams,
	DataMgr* dataMgr,
	int timeStep,
	int refLevel,
	std::vector<double> &minExts,
	std::vector<double> &maxExts) {
	
	vector<string> fieldVars = rParams->GetFieldVariableNames();
	for (int i=0; i<3; i++) {
		std::vector<double> tmpMin, tmpMax;
		string varName = fieldVars[i];
		if (varName == "") 
			continue;

		dataMgr->GetVariableExtents(timeStep, varName, refLevel,
			tmpMin, tmpMax);

		if (minExts.size() == 0) {
			for (int j=0; j<3; j++) {
				minExts = tmpMin;
				maxExts = tmpMax;
			}
		}
		else {
			for (int j=0; j<3; j++) {
				if (tmpMin[j] < minExts[j])
					minExts[j] = tmpMin[j];
				if (tmpMax[j] > maxExts[j])
					maxExts[j] = tmpMax[j];
			}
		}
	}
}

void VizWin::_getActiveExtents(
	std::vector<double> &minExts,
	std::vector<double> &maxExts
) {
	VAPoR::RenderParams* rParams = _getRenderParams();
	if (rParams == NULL) return;

	int refLevel = rParams->GetRefinementLevel();
	string varName = rParams->GetVariableName();
	vector<string> fieldVars = rParams->GetFieldVariableNames();

	ParamsMgr* paramsMgr = _controlExec->GetParamsMgr();
	AnimationParams* aParams = (AnimationParams*) paramsMgr->GetParams(
		AnimationParams::GetClassType()
	);
	int timeStep = aParams->GetCurrentTimestep();

	DataStatus* dataStatus = _controlExec->GetDataStatus();
    string dataMgrName = _getCurrentDataMgrName();
	DataMgr* dataMgr = dataStatus->GetDataMgr(dataMgrName);
	
	if (fieldVars[0]=="" && fieldVars[1]=="" && fieldVars[2]=="") {
		dataMgr->GetVariableExtents(
			timeStep, varName, refLevel, minExts, maxExts
		);
	}
	else {
		_getUnionOfFieldVarExtents(rParams, dataMgr, timeStep, 
			refLevel, minExts, maxExts
		);
	}
}

VAPoR::Transform* VizWin::_getDataMgrTransform() const {
    string dataMgrName = _getCurrentDataMgrName();
	if (dataMgrName.empty())
		return NULL;

	ParamsMgr* paramsMgr = _controlExec->GetParamsMgr();
    ViewpointParams *vpParams = paramsMgr->GetViewpointParams(_winName);
    vector<string> names = paramsMgr->GetDataMgrNames();
    VAPoR::Transform *t = vpParams->GetTransform(dataMgrName);
    return t;
}

void VizWin::updateManip(bool initialize) {

	std::vector<double> minExts(3,numeric_limits<double>::max());
	std::vector<double> maxExts(3,numeric_limits<double>::lowest());

	_getActiveExtents(minExts, maxExts);

	std::vector<double> llc, urc;
	string classType;
	VAPoR::RenderParams* rParams = _getRenderParams(classType);
	if (initialize || rParams==NULL) {
		llc = minExts;
		urc = maxExts;
	}
	else {
		VAPoR::Box* box = rParams->GetBox();
		box->GetExtents(llc, urc);
	}

	bool constrain = true;
	if (classType == ImageParams::GetClassType()) 
		constrain = false;

	VAPoR::Transform* dmTransform = _getDataMgrTransform();
	VAPoR::Transform* rpTransform = NULL;
	if (rParams != NULL)
		rpTransform = rParams->GetTransform();
    
	_manip->Update(
		llc, urc, minExts, 
		maxExts, rpTransform, dmTransform, 
		constrain
	);

    if (!initialize)
        _manip->Render();
    GL_ERR_BREAK();
}
