//-- AnnotationRenderer.cpp ----------------------------------------------------------
//   
//				   Copyright (C)  2015
//	 University Corporation for Atmospheric Research
//				   All Rights Reserved
//
//----------------------------------------------------------------------------
//
//	  File:		   AnnotationRenderer.cpp
//
//	  Author:		 Alan Norton
//
//	  Description:  Implementation of AnnotationRenderer class
//
//----------------------------------------------------------------------------


#include <vapor/glutil.h>	// Must be included first!!!
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include <sstream>
#include <iomanip>

#ifndef WIN32
#include <unistd.h>
#endif


#include <vapor/DataStatus.h>
#include <vapor/AnnotationRenderer.h>
#include <vapor/GetAppPath.h>
#include "vapor/GLManager.h"
#include "vapor/LegacyGL.h"
#include "vapor/TextLabel.h"

using namespace VAPoR;
using namespace Wasp;

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
AnnotationRenderer::AnnotationRenderer(
	const ParamsMgr *pm, const DataStatus *dataStatus, string winName
) {
	m_paramsMgr = pm;
	m_dataStatus = dataStatus;
	m_winName = winName;
    _glManager = nullptr;

	_currentTimestep = 0;

    _fontName = "arimo";
	vector<string> fpath;
	fpath.push_back("fonts");
	_fontFile = GetAppPath("VAPOR", "share", fpath);
	_fontFile = _fontFile + "//" + _fontName + ".ttf";
}

//----------------------------------------------------------------------------
//
//----------------------------------------------------------------------------
AnnotationRenderer::~AnnotationRenderer()
{
#ifdef	VAPOR3_0_0_ALPHA
	if (_textObjectsValid) invalidateCache();
#endif
}

void AnnotationRenderer::InitializeGL(GLManager *glManager) {
    _glManager = glManager;
}

//Issue OpenGL commands to draw a grid of lines of the full domain.
//Grid resolution is up to 2x2x2
//
void AnnotationRenderer::drawDomainFrame(size_t ts) const {

	AnnotationParams *vfParams = m_paramsMgr->GetAnnotationParams(m_winName);

	vector <double> minExts, maxExts;
	m_dataStatus->GetActiveExtents(
		m_paramsMgr, m_winName, ts, minExts, maxExts
	);

	int i; 
	int numLines[3];
	double fullSize[3], modMin[3],modMax[3];

	//Instead:  either have 2 or 1 lines in each dimension.  2 if the size is < 1/3
	for (i = 0; i<minExts.size(); i++){
		double regionSize = maxExts[i]-minExts[i];

		//Stretch size by 1%
		fullSize[i] = (maxExts[i] - minExts[i])*1.01;
		double mid = 0.5f*(maxExts[i]+minExts[i]);
		modMin[i] = mid - 0.5f*fullSize[i];
		modMax[i] = mid + 0.5f*fullSize[i];
		if (regionSize < fullSize[i]*.3) numLines[i] = 2;
		else numLines[i] = 1;
		
	}
	
	double clr[3];
	vfParams->GetDomainColor(clr);
	glLineWidth(1);
	//Now draw the lines.  Divide each dimension into numLines[dim] sections.

	int x,y,z;
	//Do the lines in each z-plane
	//Turn on writing to the z-buffer
	glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
	
    LegacyGL *lgl = _glManager->legacy;
	lgl->Color3f(clr[0], clr[1], clr[2]);
    lgl->Begin(GL_LINES);
	for (z = 0; z<=numLines[2]; z++){
		float zCrd = modMin[2] + ((float)z/(float)numLines[2])*fullSize[2];
		//Draw lines in x-direction for each y
		for (y = 0; y<=numLines[1]; y++){
			float yCrd = modMin[1] + ((float)y/(float)numLines[1])*fullSize[1];
			
			lgl->Vertex3f(  modMin[0],  yCrd, zCrd );
			lgl->Vertex3f( modMax[0],  yCrd, zCrd );
		}
		//Draw lines in y-direction for each x
		for (x = 0; x<=numLines[0]; x++){
			float xCrd = modMin[0] + ((float)x/(float)numLines[0])*fullSize[0];
			
			lgl->Vertex3f(  xCrd, modMin[1], zCrd );
			lgl->Vertex3f( xCrd, modMax[1], zCrd );
		}
	}
	//Do the lines in each y-plane
    
	for (y = 0; y<=numLines[1]; y++){
		float yCrd = modMin[1] + ((float)y/(float)numLines[1])*fullSize[1];
		//Draw lines in x direction for each z
		for (z = 0; z<=numLines[2]; z++){
			float zCrd = modMin[2] + ((float)z/(float)numLines[2])*fullSize[2];
			
			lgl->Vertex3f(  modMin[0],  yCrd, zCrd );
			lgl->Vertex3f( modMax[0],  yCrd, zCrd );
			
		}
		//Draw lines in z direction for each x
		for (x = 0; x<=numLines[0]; x++){
			float xCrd = modMin[0] + ((float)x/(float)numLines[0])*fullSize[0];
		
			lgl->Vertex3f(  xCrd, yCrd, modMin[2] );
			lgl->Vertex3f( xCrd, yCrd, modMax[2]);
			
		}
	}
	
	//Do the lines in each x-plane
	for (x = 0; x<=numLines[0]; x++){
		float xCrd = modMin[0] + ((float)x/(float)numLines[0])*fullSize[0];
		//Draw lines in y direction for each z
		for (z = 0; z<=numLines[2]; z++){
			float zCrd = modMin[2] + ((float)z/(float)numLines[2])*fullSize[2];
			
			lgl->Vertex3f(  xCrd, modMin[1], zCrd );
			lgl->Vertex3f( xCrd, modMax[1], zCrd );
			
		}
		//Draw lines in z direction for each y
		for (y = 0; y<=numLines[1]; y++){
			float yCrd = modMin[1] + ((float)y/(float)numLines[1])*fullSize[1];
			
			lgl->Vertex3f(  xCrd, yCrd, modMin[2] );
			lgl->Vertex3f( xCrd, yCrd, modMax[2]);
			
		}
	}
    lgl->End();
    
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
}

void AnnotationRenderer::DrawText()
{
    _glManager->PixelCoordinateSystemPush();
    
	DrawText(_miscAnnot);
	DrawText(_timeAnnot);
	DrawText(_axisAnnot);
    
    _glManager->PixelCoordinateSystemPop();
}

void AnnotationRenderer::DrawText(vector<billboard> billboards) {
	float txtColor[] = {1.f, 1.f, 1.f, 1.f};
	float bgColor[] = {0.f, 0.f, 0.f, 0.f};
	float coords[] = {67.5f,31.6f,0.f};

	for (int i=0; i<billboards.size(); i++) {
		string text = billboards[i].text;
		coords[0] = billboards[i].x;
		coords[1] = billboards[i].y;
		int size = billboards[i].size;
		txtColor[0] = billboards[i].color[0];
		txtColor[1] = billboards[i].color[1];
		txtColor[2] = billboards[i].color[2];
        
        TextLabel label(_glManager, _fontName, size);
        label.ForegroundColor = glm::make_vec4(txtColor);
        label.BackgroundColor = glm::make_vec4(bgColor);
        label.DrawText(glm::make_vec2(coords), text);
	}
}

void AnnotationRenderer::AddText(string text, 
								int x, int y, int size, 
								float color[3], int type) {
	//_billboards.clear();  // Temporary hack.  We eventually need separate
							// billboard groups for time annotations, axis
							// labels, etc.  Grouping them all in the same
							// vector makes it hard if not impossible to 
							// make changes to any of the labels (color, size,
							// etc)
	billboard myBoard;
	myBoard.text = text;
	myBoard.x = x;
	myBoard.y = y;
	myBoard.size = size;
	myBoard.color[0] = color[0];
	myBoard.color[1] = color[1];
	myBoard.color[2] = color[2];

	if (type == 0) {	// Miscellaneous annotation
		_miscAnnot.push_back(myBoard);
	}
	else if (type == 1) {	// Time annotation
		_timeAnnot.push_back(myBoard);
	}
	else if (type == 2) {
		_axisAnnot.push_back(myBoard);
	}
}

void AnnotationRenderer::ClearText(int type) {
	if (type==-1) {
		_miscAnnot.clear();
		_timeAnnot.clear();
		_axisAnnot.clear();
	}
	if (type==0) {
		_miscAnnot.clear();
	}
	else if (type==1) {
		_timeAnnot.clear();
	}
	else if (type==2) {
		_axisAnnot.clear();
	}
}

void AnnotationRenderer::applyTransform(Transform *t) {
	vector<double> scale = t->GetScales();
	vector<double> origin = t->GetOrigin();
	vector <double> translate = t->GetTranslations();
	vector <double> rotate  = t->GetRotations();
	assert(translate.size() == 3); 
	assert(rotate.size()	== 3); 
	assert(scale.size()  == 3); 
	assert(origin.size()	== 3); 

	_glManager->matrixManager->Translate(origin[0], origin[1], origin[2]); // glTranslatef(origin[0], origin[1], origin[2]);
	_glManager->matrixManager->Scale(scale[0], scale[1], scale[2]); // glScalef(scale[0], scale[1], scale[2]);
	_glManager->matrixManager->Rotate(rotate[0], 1, 0, 0); // glRotatef(rotate[0], 1, 0, 0);
	_glManager->matrixManager->Rotate(rotate[1], 0, 1, 0); // glRotatef(rotate[1], 0, 1, 0);
	_glManager->matrixManager->Rotate(rotate[2], 0, 0, 1); // glRotatef(rotate[2], 0, 0, 1);
	_glManager->matrixManager->Translate(-origin[0], -origin[1], -origin[2]); // glTranslatef(-origin[0], -origin[1], -origin[2]);

	_glManager->matrixManager->Translate(translate[0], translate[1], translate[2]); // glTranslatef(translate[0], translate[1], translate[2]);
}

void AnnotationRenderer::InScenePaint(size_t ts)
{
	AnnotationParams *vfParams = m_paramsMgr->GetAnnotationParams(m_winName);
    MatrixManager *mm = _glManager->matrixManager;

    _currentTimestep = ts;

	// Push or reset state
	mm->MatrixModeModelView();
	mm->PushMatrix();
	// glPushAttrib(GL_ALL_ATTRIB_BITS); // TODO GL
	
	vector<string> winNames = m_paramsMgr->GetVisualizerNames();
	ViewpointParams *vpParams = m_paramsMgr->GetViewpointParams(m_winName);

	vector<string> names = m_paramsMgr->GetDataMgrNames();
	Transform *t = vpParams->GetTransform(names[0]);
	applyTransform(t);

	double mvMatrix[16];
    mm->GetDoublev(MatrixManager::Mode::ModelView, mvMatrix);
	vpParams->SetModelViewMatrix(mvMatrix);

	if (vfParams->GetUseDomainFrame()) drawDomainFrame(ts);

	if (vfParams->GetShowAxisArrows()) {

		vector <double> minExts, maxExts;
		m_dataStatus->GetActiveExtents(
			m_paramsMgr, m_winName, ts, minExts, maxExts
		);
		drawAxisArrows(minExts, maxExts);
	}

	AxisAnnotation* aa = vfParams->GetAxisAnnotation();
	if (aa->GetAxisAnnotationEnabled()) 
		drawAxisTics(aa);
    
	mm->MatrixModeModelView();
	mm->PopMatrix();
	
    mm->GetDoublev(MatrixManager::Mode::ModelView, mvMatrix);
	vpParams->SetModelViewMatrix(mvMatrix);
    
	printOpenGLErrorMsg(m_winName.c_str());
}

void AnnotationRenderer::scaleNormalizedCoordinatesToWorld(
	std::vector<double> &coords,
	string dataMgrName
) {
	std::vector<double> extents = getDomainExtents();
	int dims = extents.size()/2;
	for (int i=0; i<dims; i++) {
		double offset = coords[i]*(extents[i+dims]-extents[i]);
		double minimum = extents[i];
		coords[i] = offset + minimum;
	}
}

void AnnotationRenderer::drawAxisTics(AxisAnnotation* aa) {
	
	if (aa==NULL)
		aa = getCurrentAxisAnnotation();

	// Preserve the current GL color state
	// glPushAttrib(GL_CURRENT_BIT); // TODO GL

	vector<double> origin = aa->GetAxisOrigin();
	vector<double> minTic = aa->GetMinTics();
	vector<double> maxTic = aa->GetMaxTics();

	string dmName = aa->GetDataMgrName();
	scaleNormalizedCoordinatesToWorld(origin, dmName);
	scaleNormalizedCoordinatesToWorld(minTic, dmName);
	scaleNormalizedCoordinatesToWorld(maxTic, dmName);
	
	vector<double> ticLength = aa->GetTicSize();
	vector<double> ticDir = aa->GetTicDirs();
	vector<double> numTics = aa->GetNumTics();
	vector<double> axisColor = aa->GetAxisColor();
	double width = aa->GetTicWidth();
	bool latLon = aa->GetLatLonAxesEnabled();

	_drawAxes(minTic, maxTic, origin, axisColor, width);	
	
	double pointOnAxis[3]; 
	double ticVec[3];
	double startPosn[3], endPosn[3];
	vector<double> extents = getDomainExtents();

	//Now draw tic marks for x:
	pointOnAxis[1] = origin[1];
	pointOnAxis[2] = origin[2];
	ticVec[0] = 0.f; ticVec[1] = 0.f; ticVec[2] = 0.f;
	double scaleFactor;
	if (ticDir[0] == 1) {	// Y orientation
		scaleFactor = extents[4]-extents[1];
		ticVec[1] = ticLength[0]*scaleFactor; 
	}
	else {					// Z orientation
		scaleFactor = extents[5]-extents[2];
		ticVec[2] = ticLength[0]*scaleFactor;
	}
	//ticVec[1] = ticLength[1]*scaleFactor;
	//ticVec[2] = ticLength[2]*scaleFactor;
	for (int i = 0; i< numTics[0]; i++){
		pointOnAxis[0] = minTic[0] + (float)i* (maxTic[0] - minTic[0])/(float)(numTics[0]-1);
		vsub(pointOnAxis, ticVec, startPosn);
		vadd(pointOnAxis, ticVec, endPosn);
	
		//_drawTic(startPosn, endPosn, length, width, axisColor);
		_drawTic(startPosn, endPosn, width, axisColor);
		
		double text = pointOnAxis[0];
		if (latLon)
			convertPointToLon(text);
		renderText(text, startPosn, aa);
	}
	
	//Now draw tic marks for y:
	pointOnAxis[0] = origin[0];
	pointOnAxis[2] = origin[2];
	ticVec[0] = 0.f; ticVec[1] = 0.f; ticVec[2] = 0.f;
	if (ticDir[1] == 2) {	// Z orientation
		scaleFactor = extents[5]-extents[2];
		ticVec[2] = ticLength[1]*scaleFactor;
	}
	else {					// X orientation
		scaleFactor = extents[4]-extents[1];
		ticVec[0] = ticLength[1]*scaleFactor; 
	}
	for (int i = 0; i< numTics[1]; i++){
		pointOnAxis[1] = minTic[1] + (float)i* (maxTic[1] - minTic[1])/(float)(numTics[1]-1);
		vsub(pointOnAxis, ticVec, startPosn);
		vadd(pointOnAxis, ticVec, endPosn);
		
		_drawTic(startPosn, endPosn, width, axisColor);

		double text = pointOnAxis[1];
		if (latLon)
			convertPointToLat(text);
		renderText(text, startPosn, aa);
	}

	//Now draw tic marks for z:
	pointOnAxis[0] = origin[0];
	pointOnAxis[1] = origin[1];
	ticVec[0] = 0.f; ticVec[1] = 0.f; ticVec[2] = 0.f;
	if (ticDir[2] == 1) {	// Y orientation
		scaleFactor = extents[4]-extents[1];
		ticVec[1] = ticLength[2]*scaleFactor;
	}
	else {					// X orientation
		scaleFactor = extents[3]-extents[0];
		ticVec[0] = ticLength[2]*scaleFactor; 
	}
	for (int i = 0; i< numTics[2]; i++){
		pointOnAxis[2] = minTic[2] + (float)i* (maxTic[2] - minTic[2])/(float)(numTics[2]-1);
		vsub(pointOnAxis, ticVec, startPosn);
		vadd(pointOnAxis, ticVec, endPosn);
		_drawTic(startPosn, endPosn, width, axisColor);
		renderText(pointOnAxis[2], startPosn, aa);
	}
}

void AnnotationRenderer::_drawAxes(
	std::vector<double> min, 
	std::vector<double> max, 
	std::vector<double> origin,
	std::vector<double> color,
	double width) {
	
    LegacyGL *lgl = _glManager->legacy;
    
	// glPushAttrib(GL_CURRENT_BIT);	// TODO GL
	GL_LEGACY(glDisable(GL_LIGHTING));
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	lgl->Color4f(color[0],color[1], color[2], color[3]);
	GL_LEGACY(glLineWidth(width));
	glEnable(GL_LINE_SMOOTH);
	lgl->Begin(GL_LINES);
	lgl->Vertex3f(min[0],origin[1],origin[2]);
	lgl->Vertex3f(max[0],origin[1],origin[2]);
	lgl->Vertex3f(origin[0],min[1],origin[2]);
	lgl->Vertex3f(origin[0],max[1],origin[2]);
	lgl->Vertex3f(origin[0],origin[1],min[2]);
	lgl->Vertex3f(origin[0],origin[1],max[2]);
	lgl->End();
	glDisable(GL_LINE_SMOOTH);
	//glEnable(GL_LIGHTING);
	// glPopAttrib(); // TODO GL
}

void AnnotationRenderer::_drawTic(
	double startPosn[],
	double endPosn[],
	double width,
	std::vector<double> color
){
	// glPushAttrib(GL_CURRENT_BIT); // TODO GL
    LegacyGL *lgl = _glManager->legacy;
	lgl->Color4f(color[0], color[1], color[2], color[3]);
	glLineWidth(width);
	glEnable(GL_LINE_SMOOTH);
    lgl->Begin(GL_LINES);
	lgl->Vertex3dv(startPosn);
	lgl->Vertex3dv(endPosn);
	lgl->End();
	glDisable(GL_LINE_SMOOTH);
	// glPopAttrib(); // TODO GL
}

void AnnotationRenderer::convertPointToLon(double &xCoord) {
	double dummy = 0.;
	convertPointToLonLat(xCoord, dummy); 
}

void AnnotationRenderer::convertPointToLat(double &yCoord) {
	double dummy = 0.;
	convertPointToLonLat(dummy, yCoord); 
}

void AnnotationRenderer::convertPointToLonLat(
    double &xCoord, double &yCoord
) {
	double coords[2] = {xCoord, yCoord};
	double coordsForError[2] = {coords[0], coords[1]};

	string projString = m_dataStatus->GetMapProjection();
	int rc = DataMgrUtils::ConvertPCSToLonLat(projString, coords, 1);
	if (!rc) {
		MyBase::SetErrMsg("Could not convert point %f, %f to Lon/Lat",
			coordsForError[0], coordsForError[1]);
 	}

	xCoord = coords[0];
	yCoord = coords[1];
}

Transform* AnnotationRenderer::getTransform(string dataMgrName) {
	if (dataMgrName=="")
		dataMgrName = getCurrentDataMgrName();

	ViewpointParams *vpParams = m_paramsMgr->GetViewpointParams(m_winName);
	vector<string> names = m_paramsMgr->GetDataMgrNames();
	Transform *t = vpParams->GetTransform(names[0]);
	return t;
}

AxisAnnotation* AnnotationRenderer::getCurrentAxisAnnotation() {
	AnnotationParams *vfParams = m_paramsMgr->GetAnnotationParams(m_winName);
	string currentAxisDataMgr = vfParams->GetCurrentAxisDataMgrName();
	AxisAnnotation* aa = vfParams->GetAxisAnnotation();
	return aa;
}

string AnnotationRenderer::getCurrentDataMgrName() const {
	AnnotationParams *vfParams = m_paramsMgr->GetAnnotationParams(m_winName);
	string currentAxisDataMgr = vfParams->GetCurrentAxisDataMgrName();
	return currentAxisDataMgr;
}

std::vector<double> AnnotationRenderer::getDomainExtents() const {
	int ts = _currentTimestep;
	vector <double> minExts, maxExts;

	m_dataStatus->GetActiveExtents(
		m_paramsMgr, ts, minExts, maxExts
	);

	std::vector<double> extents;
	for (int i=0; i<minExts.size(); i++) {
		extents.push_back(minExts[i]);
	}
	for (int i=0; i<maxExts.size(); i++) {
		extents.push_back(maxExts[i]);
	}

	return extents;
}

void AnnotationRenderer::renderText(
	double text, 
	double coord[],
	AxisAnnotation* aa)
{
	if (aa == NULL)
		aa = getCurrentAxisAnnotation();
    
    std::vector<double> axisColor = aa->GetAxisColor();
    std::vector<double> backgroundColor = aa->GetAxisBackgroundColor();
    int fontSize = aa->GetAxisFontSize();
    
    int precision = (int)aa->GetAxisDigits();
    std::stringstream ss;
    ss << fixed << setprecision(precision) << text;
    string textString = ss.str();
    
    TextLabel label(_glManager, _fontName, fontSize);
    label.HorizontalAlignment = TextLabel::Center;
    label.VerticalAlignment = TextLabel::Center;
    label.Padding = fontSize/4.f;
    label.ForegroundColor = glm::vec4(axisColor[0],
                                      axisColor[1],
                                      axisColor[2],
                                      axisColor[3]);
    label.BackgroundColor = glm::vec4(backgroundColor[0],
                                      backgroundColor[1],
                                      backgroundColor[2],
                                      backgroundColor[3]);
    label.DrawText(glm::vec3(coord[0], coord[1], coord[2]), textString);
    
    /*
	if (_textObject!=NULL) {
		delete _textObject;
		_textObject = NULL;
	}   
	_textObject = new TextObject();
	_textObject->Initialize(
		_fontFile, textString, fontSize,
		axisColor, txtBackground, vpParams, TextObject::BILLBOARD, 
		TextObject::CENTERTOP
	);

	_textObject->drawMe(coord);

	return;
     */
}

void AnnotationRenderer::drawAxisArrows(
	vector <double> minExts,
	vector <double> maxExts
) {
	assert(minExts.size() == maxExts.size());
	while (minExts.size() < 3) {
		minExts.push_back(0.0);
		maxExts.push_back(0.0);
	}

	// Preserve the current GL color state
	// glPushAttrib(GL_CURRENT_BIT); // TODO GL

	float origin[3];
	float maxLen = -1.f;

	AnnotationParams *vfParams = m_paramsMgr->GetAnnotationParams(m_winName);

	vector<double> axisArrowCoords = vfParams->GetAxisArrowCoords();

	for (int i = 0; i<3; i++){
		origin[i] = minExts[i] + (axisArrowCoords[i])*(maxExts[i]-minExts[i]);
		if (maxExts[i] - minExts[i] > maxLen) {
			maxLen = maxExts[i] - minExts[i];
		}
	}
    
    LegacyGL *lgl = _glManager->legacy;
    
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	float len = maxLen*0.2f;
	lgl->Color3f(1.f,0.f,0.f);
	GL_LEGACY(glLineWidth( 4.0 ));
	glEnable(GL_LINE_SMOOTH);
	lgl->Begin(GL_LINES);
	lgl->Vertex3fv(origin);
	lgl->Vertex3f(origin[0]+len,origin[1],origin[2]);
	
	lgl->End();
	lgl->Begin(GL_TRIANGLES);
	lgl->Vertex3f(origin[0]+len,origin[1],origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1]+.1*len, origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1], origin[2]+.1*len);

	lgl->Vertex3f(origin[0]+len,origin[1],origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1], origin[2]+.1*len);
	lgl->Vertex3f(origin[0]+.8*len, origin[1]-.1*len, origin[2]);

	lgl->Vertex3f(origin[0]+len,origin[1],origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1]-.1*len, origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1], origin[2]-.1*len);

	lgl->Vertex3f(origin[0]+len,origin[1],origin[2]);
	lgl->Vertex3f(origin[0]+.8*len, origin[1], origin[2]-.1*len);
	lgl->Vertex3f(origin[0]+.8*len, origin[1]+.1*len, origin[2]);
	lgl->End();

	lgl->Color3f(0.f,1.f,0.f);
	lgl->Begin(GL_LINES);
	lgl->Vertex3fv(origin);
	lgl->Vertex3f(origin[0],origin[1]+len,origin[2]);
	lgl->End();
	lgl->Begin(GL_TRIANGLES);
	lgl->Vertex3f(origin[0],origin[1]+len,origin[2]);
	lgl->Vertex3f(origin[0]+.1*len, origin[1]+.8*len, origin[2]);
	lgl->Vertex3f(origin[0], origin[1]+.8*len, origin[2]+.1*len);

	lgl->Vertex3f(origin[0],origin[1]+len,origin[2]);
	lgl->Vertex3f(origin[0], origin[1]+.8*len, origin[2]+.1*len);
	lgl->Vertex3f(origin[0]-.1*len, origin[1]+.8*len, origin[2]);

	lgl->Vertex3f(origin[0],origin[1]+len,origin[2]);
	lgl->Vertex3f(origin[0]-.1*len, origin[1]+.8*len, origin[2]);
	lgl->Vertex3f(origin[0], origin[1]+.8*len, origin[2]-.1*len);

	lgl->Vertex3f(origin[0],origin[1]+len,origin[2]);
	lgl->Vertex3f(origin[0], origin[1]+.8*len, origin[2]-.1*len);
	lgl->Vertex3f(origin[0]+.1*len, origin[1]+.8*len, origin[2]);
	lgl->End();
	lgl->Color3f(0.f,0.3f,1.f);
	lgl->Begin(GL_LINES);
	lgl->Vertex3fv(origin);
	lgl->Vertex3f(origin[0],origin[1],origin[2]+len);
	lgl->End();
	lgl->Begin(GL_TRIANGLES);
	lgl->Vertex3f(origin[0],origin[1],origin[2]+len);
	lgl->Vertex3f(origin[0]+.1*len, origin[1], origin[2]+.8*len);
	lgl->Vertex3f(origin[0], origin[1]+.1*len, origin[2]+.8*len);

	lgl->Vertex3f(origin[0],origin[1],origin[2]+len);
	lgl->Vertex3f(origin[0], origin[1]+.1*len, origin[2]+.8*len);
	lgl->Vertex3f(origin[0]-.1*len, origin[1], origin[2]+.8*len);

	lgl->Vertex3f(origin[0],origin[1],origin[2]+len);
	lgl->Vertex3f(origin[0]-.1*len, origin[1], origin[2]+.8*len);
	lgl->Vertex3f(origin[0], origin[1]-.1*len, origin[2]+.8*len);

	lgl->Vertex3f(origin[0],origin[1],origin[2]+len);
	lgl->Vertex3f(origin[0], origin[1]-.1*len, origin[2]+.8*len);
	lgl->Vertex3f(origin[0]+.1*len, origin[1], origin[2]+.8*len);
	lgl->End();
	
	glDisable(GL_LINE_SMOOTH);
}

