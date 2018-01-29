// variables for storing compositing results
vec4 result = vec4(0.0);

#define SAMPLING_BASE_INTERVAL_RCP 200.0

struct VOLUME_STRUCT {
    sampler3D volume_;              // the actual dataset normalized
    vec3 datasetDimensions_;        // the dataset's resolution, e.g. [ 256.0, 128.0, 128.0]
    vec3 datasetDimensionsRCP_;     // Reciprocal of the dataset dimension (= 1/datasetDimensions_)
};

struct TEXTURE_PARAMETERS {
    vec2 dimensions_;        // the texture's resolution, e.g. [256.0, 128.0]
    vec2 dimensionsRCP_;
};

struct LIGHT_SOURCE {
    vec3 position_;        // light position in world space
    vec3 ambientColor_;    // ambient color (r,g,b)
    vec3 diffuseColor_;    // diffuse color (r,g,b)
    vec3 specularColor_;   // specular color (r,g,b)
    vec3 attenuation_;     // attenuation (constant, linear, quadratic)
};

// uniforms needed for shading
uniform vec3 cameraPosition_;   // in world coordinates
uniform float shininess_;       // material shininess parameter
uniform LIGHT_SOURCE lightSource_;

// Settings for the raycaster
uniform float samplingStepSize_;
uniform float samplingRate_; 

// declare entry and exit parameters
uniform sampler2D entryPoints_;            // ray entry points
uniform TEXTURE_PARAMETERS entryParameters_;
uniform sampler2D exitPoints_;             // ray exit points
uniform TEXTURE_PARAMETERS exitParameters_;

// declare volume
uniform VOLUME_STRUCT volumeStruct_;    // volume data with parameters

// delcare transfer function
uniform sampler1D transferFunc_;

/////////////////////////////////////////////////////

vec3 calculateGradient(in vec3 samplePosition) {
    const vec3 h = volumeStruct_.datasetDimensionsRCP_;
    // Implement central differences
    
    //task 2
    //*********************************************************
    float fxp1 = texture(volumeStruct_.volume_, vec3(samplePosition.x + h.x, samplePosition.y, samplePosition.z)).a;
    float fxp2 = texture(volumeStruct_.volume_, vec3(samplePosition.x - h.x, samplePosition.y, samplePosition.z)).a;
    
    float fyp1 = texture(volumeStruct_.volume_, vec3(samplePosition.x, samplePosition.y + h.y,  samplePosition.z)).a;
    float fyp2 = texture(volumeStruct_.volume_, vec3(samplePosition.x, samplePosition.y - h.y,  samplePosition.z)).a; 
    
    float fzp1 = texture(volumeStruct_.volume_, vec3(samplePosition.x, samplePosition.y, samplePosition.z + h.z)).a;
    float fzp2 = texture(volumeStruct_.volume_, vec3(samplePosition.x, samplePosition.y, samplePosition.z - h.z)).a;
       
    float xPos = fxp1 - fxp2;
    float yPos = fyp1 - fyp2;
    float zPos = fzp1 - fzp2;
  //*********************************************************
    //check if sample outside borders, if true, set gradient to 0.
    if(samplePosition.x + h.x > 1 || samplePosition.x - h.x < 0)
      xPos = 0.0;
      
    if(samplePosition.y + h.y > 1 || samplePosition.y - h.y < 0)
      yPos = 0.0;
      
    if(samplePosition.z + h.z > 1 || samplePosition.z - h.z < 0)
      zPos = 0.0;
    
    vec3 gradient = (1/(2*h)) * vec3(xPos, yPos, zPos);
 
    return gradient;
    //*********************************************************  
}

//task3
//*********************************************************
//Diffuse Lightning
vec3 getDiffuseColor(in vec3 kd, in vec3 G, in vec3 L){
    float GdotL = max(dot(G, L), 0.0);
    return kd * lightSource_.diffuseColor_.rgb * GdotL;
}

//Specular Lightning
vec3 getSpecularColor(in vec3 ks, in vec3 G, in vec3 L, in vec3 V){
    vec3 H = normalize(V + L);
    float GdotH = pow(max(dot(G, H), 0.0), shininess_);
    return ks * lightSource_.specularColor_.rgb * GdotH;
    
    //matParams
}

//ambient lightning
vec3 getAmbientColor(in vec3 ka) {
    return ka * lightSource_.ambientColor_.rgb;
    
    //lightParams
}
//*********************************************************

vec3 applyPhongShading(in vec3 pos, in vec3 gradient, in vec3 ka, in vec3 kd, in vec3 ks) {
    // Implement phong shading
    
    //task 3
    //*********************************************************
    vec3 L = normalize(lightSource_.position_ - pos);
    vec3 V = normalize(cameraPosition_ - pos);
      
    vec3 shadedColor = vec3(0.0);
    shadedColor += getDiffuseColor(kd, normalize(gradient), L);
    shadedColor += getSpecularColor(ks, normalize(gradient), L, V);
    shadedColor += getAmbientColor(ka);
      
    return shadedColor;
    //*********************************************************
}

void rayTraversal(in vec3 first, in vec3 last) {
    // calculate the required ray parameters
    float t     = 0.0;
    float tIncr = 0.0;
    float tEnd  = 1.0;
    vec3 rayDirection = last - first;
    tEnd = length(rayDirection);
    rayDirection = normalize(rayDirection);
    tIncr = 1.0/(samplingRate_ * length(rayDirection*volumeStruct_.datasetDimensions_));
    
    bool finished = false;
    while (!finished) {
        vec3 samplePos = first + t * rayDirection;
        float intensity = texture(volumeStruct_.volume_, samplePos).a;
        
        vec3 gradient = calculateGradient(samplePos);

        vec4 color = texture(transferFunc_, intensity);
        
        color.rgb = applyPhongShading(samplePos, gradient, color.rgb, color.rgb, vec3(1.0,1.0,1.0));
        
        // if opacity greater zero, apply compositing
        if (color.a > 0.0) {
            color.a = 1.0 - pow(1.0 - color.a, samplingStepSize_ * SAMPLING_BASE_INTERVAL_RCP);
            // Insert your front-to-back alpha compositing code here
            
            //task 1
            //*********************************************************
	    result.rgb = result.rgb*result.a + (1 - result.a)*color.rgb;
	    result.a = result.a + (1 - result.a)*color.a;
	    //*********************************************************
        }

        // early ray termination
        if (result.a > 1.0)
            finished = true;
        
        t += tIncr;
        finished = finished || (t > tEnd);
    }
}

void main() {
    vec3 frontPos = texture(entryPoints_, gl_FragCoord.xy * entryParameters_.dimensionsRCP_).rgb;
    vec3 backPos = texture(exitPoints_, gl_FragCoord.xy * exitParameters_.dimensionsRCP_).rgb;

    // determine whether the ray has to be casted
    if (frontPos == backPos)
        // background needs no raycasting
        discard;
    else
        // fragCoords are lying inside the bounding box
        rayTraversal(frontPos, backPos);

    FragData0 = result;
}
