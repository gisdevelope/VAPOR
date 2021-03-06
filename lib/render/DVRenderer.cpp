#include "vapor/DVRenderer.h"
#include "vapor/GetAppPath.h"

using namespace VAPoR;

//
// Register class with object factory!!!
//
static RendererRegistrar<DVRenderer> registrar( DVRenderer::GetClassType(), 
                                                DVRParams::GetClassType() );


DVRenderer::DVRenderer( const ParamsMgr*    pm,
                        std::string&        winName,
                        std::string&        dataSetName,
                        std::string&        instName,
                        DataMgr*            dataMgr )
          : RayCaster(  pm,
                        winName,
                        dataSetName,
                        DVRParams::GetClassType(),
                        DVRenderer::GetClassType(),
                        instName,
                        dataMgr )
{ }

void DVRenderer::_loadShaders()
{
    std::vector< std::string > extraPath;
    extraPath.push_back("shaders");
    extraPath.push_back("main");
    std::string shaderPath      = Wasp::GetAppPath( "VAPOR", "share", extraPath );
    std::string VShader1stPass  = shaderPath + "/DVR1stPass.vgl";
    std::string FShader1stPass  = shaderPath + "/DVR1stPass.fgl";
    std::string VShader2ndPass  = shaderPath + "/DVR2ndPass.vgl";
    std::string FShader2ndPass  = shaderPath + "/DVR2ndPass.fgl";
    std::string VShader3rdPass  = shaderPath + "/DVR3rdPass.vgl";
    std::string FShader3rdPass  = shaderPath + "/DVR3rdPass.fgl";

    _1stPassShaderId  = _compileShaders( VShader1stPass.data(), FShader1stPass.data() );
    _2ndPassShaderId  = _compileShaders( VShader2ndPass.data(), FShader2ndPass.data() );
    _3rdPassShaderId  = _compileShaders( VShader3rdPass.data(), FShader3rdPass.data() );
}


void DVRenderer::_3rdPassSpecialHandling( bool fast )
{
    // Special handling for DVR:
    //   turn off lighting during fast rendering.
    GLint uniformLocation = glGetUniformLocation( _3rdPassShaderId, "lighting" );
    if( fast )                      // Disable lighting during "fast" DVR rendering
        glUniform1i( uniformLocation, int(0) );
    else
    {
        RayCasterParams* params = dynamic_cast<RayCasterParams*>( GetActiveParams() );
        bool lighting           = params->GetLighting();
        glUniform1i( uniformLocation, int(lighting) );

        if( lighting )
        {
            std::vector<double> coeffsD = params->GetLightingCoeffs();
            float coeffsF[4] = { (float)coeffsD[0], (float)coeffsD[1], 
                                 (float)coeffsD[2], (float)coeffsD[3] };
            uniformLocation = glGetUniformLocation( _3rdPassShaderId, "lightingCoeffs" );
            glUniform1fv( uniformLocation, (GLsizei)4, coeffsF );
        }
    }
}
