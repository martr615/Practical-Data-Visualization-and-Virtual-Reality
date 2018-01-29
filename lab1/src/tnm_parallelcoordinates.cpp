
#include "modules/tnm093/include/tnm_parallelcoordinates.h"

namespace voreen {
    
TNMParallelCoordinates::AxisHandle::AxisHandle(AxisHandlePosition location, int index, const tgt::vec2& position)
    : _location(location)
    , _index(index)
    , _position(position)
{}

void TNMParallelCoordinates::AxisHandle::setPosition(const tgt::vec2& position) {
    _position.y = position.y; //flytta bara i 
  
}

tgt::vec2 TNMParallelCoordinates::AxisHandle::getPosition() {
    return _position;
}

int TNMParallelCoordinates::AxisHandle::index() const {
    return _index;
}

void TNMParallelCoordinates::AxisHandle::render() const {
    glColor3f(1, 0, 0);
    renderInternal();
}

void TNMParallelCoordinates::AxisHandle::renderPicking() const {
	// Mapping the integer index to a float value between 1/255 and 1
    const float color = (_index + 1) / 255.f;
	// The picking information is rendered only in the red channel
    glColor3f(color, 0.f, 0.f);
    renderInternal();
}

void TNMParallelCoordinates::AxisHandle::renderInternal() const {
    const float xDiff = 0.05f;
    float yDiff;
    if (_location == AxisHandlePositionTop)
        yDiff = 0.05f;
    else if (_location == AxisHandlePositionBottom)
        yDiff = -0.05;
    glBegin(GL_TRIANGLES);
    glVertex2f(_position.x, _position.y - yDiff / 2.f);
    glVertex2f(_position.x - xDiff, _position.y + yDiff / 2.f);
    glVertex2f(_position.x + xDiff, _position.y + yDiff / 2.f);
    glEnd();
}


TNMParallelCoordinates::TNMParallelCoordinates()
    : RenderProcessor()
    , _inport(Port::INPORT, "in.data")
    , _outport(Port::OUTPORT, "out.image")
    , _privatePort(Port::OUTPORT, "private.image", false, Processor::INVALID_RESULT, GL_RGBA32F)
    , _pickedHandle(-1)
	, _brushingIndices("brushingIndices", "Brushing Indices")
	, _linkingIndices("linkingIndices", "Linking Indices")
{
    addPort(_inport);
    addPort(_outport);
    addPrivateRenderPort(_privatePort);

	addProperty(_brushingIndices);
	addProperty(_linkingIndices);

    _mouseClickEvent = new EventProperty<TNMParallelCoordinates>(
        "mouse.click", "Mouse Click",
        this, &TNMParallelCoordinates::handleMouseClick,
        tgt::MouseEvent::MOUSE_BUTTON_LEFT, tgt::MouseEvent::CLICK, tgt::Event::MODIFIER_NONE);
    addEventProperty(_mouseClickEvent);

    _mouseMoveEvent = new EventProperty<TNMParallelCoordinates>(
        "mouse.move", "Mouse Move",
        this, &TNMParallelCoordinates::handleMouseMove,
        tgt::MouseEvent::MOUSE_BUTTON_LEFT, tgt::MouseEvent::MOTION, tgt::Event::MODIFIER_NONE);
    addEventProperty(_mouseMoveEvent);

    _mouseReleaseEvent = new EventProperty<TNMParallelCoordinates>(
        "mouse.release", "Mouse Release",
        this, &TNMParallelCoordinates::handleMouseRelease,
        tgt::MouseEvent::MOUSE_BUTTON_LEFT, tgt::MouseEvent::RELEASED, tgt::Event::MODIFIER_NONE);
    addEventProperty(_mouseReleaseEvent);


    //Axis positions
    const float axisPos0 = -1.f;
    const float axisPos1 = -1.f/3.f;
    const float axisPos2 = 1.f/3.f;
    const float axisPos3 = 1.f;
    
    // Create AxisHandles here with a unique id    
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionTop, 0, tgt::vec2(axisPos0,1)));
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionBottom, 1, tgt::vec2(axisPos0,-1)));
     
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionTop, 2, tgt::vec2(axisPos1,1)));
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionBottom, 3, tgt::vec2(axisPos1,-1)));
     
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionTop, 4, tgt::vec2(axisPos2,1)));
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionBottom, 5, tgt::vec2(axisPos2,-1)));
     
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionTop, 6, tgt::vec2(axisPos3,1)));
    _handles.push_back(AxisHandle(AxisHandle::AxisHandlePositionBottom, 7, tgt::vec2(axisPos3,-1)));    

}

TNMParallelCoordinates::~TNMParallelCoordinates() {
    delete _mouseClickEvent;
    delete _mouseMoveEvent;
}

void TNMParallelCoordinates::process() {
	// Activate the user-outport as the rendering target
    _outport.activateTarget();
	// Clear the buffer
    _outport.clearTarget();

	// Render the handles
    renderHandles();
	// Render the parallel coordinates lines
    renderLines();

	// We are done with the visual part
    _outport.deactivateTarget();

	// Activate the internal port used for picking
    _privatePort.activateTarget();
	// Clear that buffer as well
    _privatePort.clearTarget();
	// Render the handles with the picking information encoded in the red channel
    renderHandlesPicking();
	// Render the lines with the picking information encoded in the green/blue/alpha channel
	renderLinesPicking();
	// We are done with the private render target
    _privatePort.deactivateTarget();
}

void TNMParallelCoordinates::handleMouseClick(tgt::MouseEvent* e) {
	// The picking texture is the result of the previous rendering in the private render port
    tgt::Texture* pickingTexture = _privatePort.getColorTexture();
	// Retrieve the texture from the graphics memory and get it to the RAM
	pickingTexture->downloadTexture();

	// The texture coordinates are flipped in the y direction, so we take care of that here
    const tgt::ivec2 screenCoords = tgt::ivec2(e->coord().x, pickingTexture->getDimensions().y - e->coord().y);
	// And then go from integer pixel coordinates to [-1,1] coordinates
    const tgt::vec2& normalizedDeviceCoordinates = (tgt::vec2(screenCoords) / tgt::vec2(_privatePort.getSize()) - 0.5f) * 2.f;

    if (normalizedDeviceCoordinates.x < -1.f || normalizedDeviceCoordinates.x > 1.f || normalizedDeviceCoordinates.y < -1.f || normalizedDeviceCoordinates.y > 1.f)
      return;
    
	// The picking information for the handles is stored in the red color channel
    int handleId = static_cast<int>(pickingTexture->texelAsFloat(screenCoords).r * 255 - 1);

    LINFOC("Picking", "Picked handle index: " << handleId);
    // Use the 'id' and the 'normalizedDeviceCoordinates' to move the correct handle
    // The id is the id of the AxisHandle that has been clicked (the same id you assigned in the constructor)
    // id == -1 if no handle was clicked
    // Use the '_pickedIndex' member variable to store the picked index
    _pickedHandle = handleId;

    //int lineId = -1;
    
    // Derive the id of the line that was clicked based on the color scheme that you devised in the
    // renderLinesPicking methodvoid TNMParallelCoordinates::renderHandlesPicking() {

    const Data* _data = _inport.getData();
    
    int lineId = static_cast<int>(pickingTexture->texelAsFloat(screenCoords).g * _data->size() - 1); // modify to use green and correct size

    
    if (lineId != -1){
      // We want to add it only if a line was clicked
	    LINFOC("Picking", "Picked line index: " << lineId);
	    _linkingList.insert(lineId);
    }
    //_linkingIndices = lineId;
	    
    // if the right mouse button is pressed and no line is clicked, clear the list:
    if ((e->button() == tgt::MouseEvent::MOUSE_BUTTON_RIGHT) && (lineId == -1))
      _linkingList.clear();

    // Make the list of selected indices available to the Scatterplot
    _linkingIndices.set(_linkingList);
}

void TNMParallelCoordinates::handleMouseMove(tgt::MouseEvent* e) {
    tgt::Texture* pickingTexture = _privatePort.getColorTexture();
    const tgt::ivec2 screenCoords = tgt::ivec2(e->coord().x, pickingTexture->getDimensions().y - e->coord().y);
    const tgt::vec2& normalizedDeviceCoordinates = (tgt::vec2(screenCoords) / tgt::vec2(_privatePort.getSize()) - 0.5f) * 2.f;

    // Move the stored index along its axis (if it is a valid picking point)
    
    const float axisPos0 = -1.f;
    const float axisPos1 = -1.f/3.f;
    const float axisPos2 = 1.f/3.f;
    const float axisPos3 = 1.f; 

	// update the _brushingList with the indices of the lines that are not rendered anymore
    
    //LINFO(_pickedHandle);
    //if clicking-dragging a upper boundy handle...
    if(_pickedHandle % 2 == 0 && _pickedHandle != -1)
    {
      //LINFO(_pickedHandle << " " << normalizedDeviceCoordinates.y << " " << _handles[_pickedHandle+1].getPosition().y);
      //check so that upper handle doesn't cross the lower one
      if(!(normalizedDeviceCoordinates.y <= _handles[_pickedHandle+1].getPosition().y))
      {
	  _handles[_pickedHandle].setPosition(normalizedDeviceCoordinates);
      }
    }
    
    //if clicking-dragging a lower boundy handle...
    else if(_pickedHandle % 2 != 0 && _pickedHandle != -1)
    {
      //check that lower handle doens't cross the upper one
      if(!(normalizedDeviceCoordinates.y >= _handles[_pickedHandle-1].getPosition().y))
      {
	_handles[_pickedHandle].setPosition(normalizedDeviceCoordinates);
      }
    }
    
    
    _brushingIndices.set(_brushingList);
 
    // This re-renders the scene (which will call process in turn)
    invalidate();
}

void TNMParallelCoordinates::handleMouseRelease(tgt::MouseEvent* e) {

}

//task 5
//**********************************************************************************************************
void TNMParallelCoordinates::renderLines() {

  // Implement your line drawing
    
    //Axis positions
    const float axisPos0 = -1.f;
    const float axisPos1 = -1.f/3.f;
    const float axisPos2 = 1.f/3.f;
    const float axisPos3 = 1.f;
  
    //get axess to all values
    const Data* _data = _inport.getData();
    
    //set initial value in case of some error
    float maxIntensity = 0;
    float maxAverage = 0;
    float maxStdDeviation = 0;
    float maxGradientMagnitude = 0;
     
    //import the values calculated in volumeinformation
    for (int i = 0; i < _data->size(); i++) {
	if (_data->at(i).dataValues[0] > maxIntensity) {
	    maxIntensity = _data->at(i).dataValues[0];
	}
	if (_data->at(i).dataValues[1] > maxAverage) {
	    maxAverage = _data->at(i).dataValues[1];
	}   
	if (_data->at(i).dataValues[2] > maxStdDeviation) {
	    maxStdDeviation = _data->at(i).dataValues[2];
	}
	if (_data->at(i).dataValues[3] > maxGradientMagnitude) {
	    maxGradientMagnitude = _data->at(i).dataValues[3];
	}
    }
    
     for (int i = 0; i < _data->size(); i++)
    {
     
      //normalize the variables because we want axis values from 0 to 1
      float normIntensity = ((_data->at(i).dataValues[0] / maxIntensity) - 0.5f) * 2.f;
      float normAverage = ((_data->at(i).dataValues[1] / maxAverage) - 0.5f) * 2.f; 
      float normStdDeviation = ((_data->at(i).dataValues[2] / maxStdDeviation) - 0.5f) * 2.f;
      float normGradientMagnitude = ((_data->at(i).dataValues[3] / maxGradientMagnitude) - 0.5f) * 2.f;
       

      //if(_brushingList.find(_data->at(i).voxelIndex) == _brushingList.end())
      //{
	
	//check that ALL values lies withing the handle boundries
	if(normIntensity <= _handles[0].getPosition().y && normIntensity >= _handles[1].getPosition().y &&
	  normAverage <= _handles[2].getPosition().y && normAverage >= _handles[3].getPosition().y &&
	  normStdDeviation <= _handles[4].getPosition().y && normStdDeviation >= _handles[5].getPosition().y &&
	  normGradientMagnitude <= _handles[6].getPosition().y && normGradientMagnitude >= _handles[7].getPosition().y)
	{ 
	  
	  //Recolor picked lines. Does unfortunately not work as intended.
	  
	  //if(_linkingList.find(_data->at(i).voxelIndex) != _linkingList.end()) { //this line works almost
	  
	  if(_linkingList.find(_data->at(i).voxelIndex) != _linkingList.end()) {	    
	      glColor4f(1,0,1,1); //purple
	  }
	  else{
	      //LINFOC("test", *(_linkingList.find(_data->at(i).voxelIndex)));
	      glColor4f(0,1,1,0.5); //turquoise
	  }

	  //draw the lines between each axis
	  glBegin(GL_LINES);
	  glVertex2f(axisPos0, normIntensity);
	  glVertex2f(axisPos1, normAverage);
	  glEnd();
	  
	  glBegin(GL_LINES);
	  glVertex2f(axisPos1, normAverage);
	  glVertex2f(axisPos2, normStdDeviation);
	  glEnd();
	  
	  glBegin(GL_LINES);
	  glVertex2f(axisPos2, normStdDeviation);
	  glVertex2f(axisPos3, normGradientMagnitude);
	  glEnd();
	}
      //}
    }
}
//**********************************************************************************************************

void TNMParallelCoordinates::renderLinesPicking() {
	// Use the same code to render lines (without duplicating it), but think of a way to encode the
	// voxel identifier into the color. The red color channel is already occupied, so you have 3
	// channels with 32-bit each at your disposal (green, blue, alpha)

    const Data* _data = _inport.getData();

    //Axis positions
    const float axisPos0 = -1.f;
    const float axisPos1 = -1.f/3.f;
    const float axisPos2 = 1.f/3.f;
    const float axisPos3 = 1.f;

    float maxIntensity = 0;
    float maxAverage = 0;
    float maxStdDeviation = 0;
    float maxGradientMagnitude = 0;
    
    //import the values calculated in volumeinformation
    for (int i = 0; i < _data->size(); i++) {
	if (_data->at(i).dataValues[0] > maxIntensity) {
	    maxIntensity = _data->at(i).dataValues[0];
	}
	if (_data->at(i).dataValues[1] > maxAverage) {
	    maxAverage = _data->at(i).dataValues[1];
	}   
	if (_data->at(i).dataValues[2] > maxStdDeviation) {
	    maxStdDeviation = _data->at(i).dataValues[2];
	}
	if (_data->at(i).dataValues[3] > maxGradientMagnitude) {
	    maxGradientMagnitude = _data->at(i).dataValues[3];
	}
    }
    
    for (int i = 0; i < _data->size(); i++)
    {
     
      //normalize the variables because we want axis values from 0 to 1
      float normIntensity = ((_data->at(i).dataValues[0] / maxIntensity) - 0.5f) * 2.f;
      float normAverage = ((_data->at(i).dataValues[1] / maxAverage) - 0.5f) * 2.f; 
      float normStdDeviation = ((_data->at(i).dataValues[2] / maxStdDeviation) - 0.5f) * 2.f;
      float normGradientMagnitude = ((_data->at(i).dataValues[3] / maxGradientMagnitude) - 0.5f) * 2.f;
       

      if(_brushingList.find(_data->at(i).voxelIndex) == _brushingList.end())
      {
	
	//check that ALL values lies withing the handle boundries
	if(normIntensity <= _handles[0].getPosition().y && normIntensity >= _handles[1].getPosition().y &&
	  normAverage <= _handles[2].getPosition().y && normAverage >= _handles[3].getPosition().y &&
	  normStdDeviation <= _handles[4].getPosition().y && normStdDeviation >= _handles[5].getPosition().y &&
	  normGradientMagnitude <= _handles[6].getPosition().y && normGradientMagnitude >= _handles[7].getPosition().y)
	{ 
	 
	  //line preferenses
	  glColor3f(0.0, float(i)/(_data->size() - 1), 0.0);
	  
	  //glColor3f(1,1,1);

	  //draw the lines between each axis
	  glBegin(GL_LINES);
	  glVertex2f(axisPos0, normIntensity);
	  glVertex2f(axisPos1, normAverage);
	  glEnd();
	  
	  glBegin(GL_LINES);
	  glVertex2f(axisPos1, normAverage);
	  glVertex2f(axisPos2, normStdDeviation);
	  glEnd();
	  
	  glBegin(GL_LINES);
	  glVertex2f(axisPos2, normStdDeviation);
	  glVertex2f(axisPos3, normGradientMagnitude);
	  glEnd();
	}
	
	else {
	  // add index to brushing list
	  // _data->at(i).voxelIndex
	}
      }
    }
  
  // id / numLines -> [0,1] -> green
}

void TNMParallelCoordinates::renderHandles() {
    for (size_t i = 0; i < _handles.size(); ++i) {
        const AxisHandle& handle = _handles[i];
        handle.render();
    }
}

void TNMParallelCoordinates::renderHandlesPicking() {
    for (size_t i = 0; i < _handles.size(); ++i) {
        const AxisHandle& handle = _handles[i];
        handle.renderPicking();
    }
}


} // namespace
