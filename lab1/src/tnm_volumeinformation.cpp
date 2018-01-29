#include "modules/tnm093/include/tnm_volumeinformation.h"
#include "voreen/core/datastructures/volume/volumeatomic.h"

namespace voreen {

	const std::string loggerCat_ = "TNMVolumeInformation";
	
namespace {
	// This ordering function allows us to sort the Data vector by the voxelIndex
	// The extraction *should* produce a sorted list, but you never know
	bool sortByIndex(const VoxelDataItem& lhs, const VoxelDataItem& rhs) {
		return lhs.voxelIndex < rhs.voxelIndex;
	}

}

TNMVolumeInformation::TNMVolumeInformation()
    : Processor()
    , _inport(Port::INPORT, "in.volume")
    , _outport(Port::OUTPORT, "out.data")
    , _data(0)
{
    addPort(_inport);
    addPort(_outport);
}

TNMVolumeInformation::~TNMVolumeInformation() {
    delete _data;
}

void TNMVolumeInformation::process() {
    const VolumeHandleBase* volumeHandle = _inport.getData();
    const Volume* baseVolume = volumeHandle->getRepresentation<Volume>();
    const VolumeUInt16* volume = dynamic_cast<const VolumeUInt16*>(baseVolume);
    if (volume == 0)
      return;
    // If we get this far, there actually is a volume to work with

    // If this is the first call, we will create the Data object
    if (_data == 0)
      _data = new Data;

    // Retrieve the size of the three dimensions of the volume
    const tgt::svec3 dimensions = volume->getDimensions();
    // Create as many data entries as there are voxels in the volume
    _data->resize(dimensions.x * dimensions.y * dimensions.z);

    // iX is the index running over the 'x' dimension
    // iY is the index running over the 'y' dimension
    // iZ is the index running over the 'z' dimension
    
    LINFOC("Volume Info", "Vol info");
    
    for (size_t iX = 0; iX < dimensions.x; ++iX) {
        for (size_t iY = 0; iY < dimensions.y; ++iY) {
            for (size_t iZ = 0; iZ < dimensions.z; ++iZ) {
	      // i is a unique identifier for the voxel calculated by the following
	      // (probably one of the most important) formulas:
	      // iZ*dimensions.x*dimensions.y + iY*dimensions.x + iX;
	      const size_t i = VolumeUInt16::calcPos(volume->getDimensions(), tgt::svec3(iX, iY, iZ));

	      // Setting the unique identifier as the voxelIndex
	      _data->at(i).voxelIndex = i;
	      //
	      // use iX, iY, iZ, i, and the VolumeUInt16::voxel method to derive the measures here
	      
	      
	      
	      // Intensity
	      // Retrieve the intensity using the 'VolumeUInt16's voxel method
	      float intensity = volume->voxel(iX,iY,iZ);
	      _data->at(i).dataValues[0] = intensity;

	      
	      
	      // Average
	      //
	      float average = 0.f;
	      // Compute the average; the voxel method accepts both a single parameter
	      // as well as three parameters
	      float sum = 0.f;
	      float values[27];
	      
	      if (iX > 0 && iY > 0 && iZ > 0 && iX < (dimensions.x - 1) && iY < (dimensions.y - 1) && iZ < (dimensions.z - 1)) 
	      {	      
		values[0] = volume->voxel(iX - 1,iY - 1,iZ - 1); values[1] = volume->voxel(iX - 1,iY - 1,iZ + 0); values[2] = volume->voxel(iX - 1,iY - 1,iZ + 1); 
		values[3] = volume->voxel(iX - 1,iY + 0,iZ - 1); values[4] = volume->voxel(iX - 1,iY + 0,iZ + 0); values[5] = volume->voxel(iX - 1,iY + 0,iZ + 1); 
		values[6] = volume->voxel(iX - 1,iY + 1,iZ - 1); values[7] = volume->voxel(iX - 1,iY + 1,iZ + 0); values[8] = volume->voxel(iX - 1,iY + 1,iZ + 1); 
		
		values[9] = volume->voxel(iX + 0,iY - 1,iZ - 1); values[10] = volume->voxel(iX + 0,iY - 1,iZ + 0); values[11] = volume->voxel(iX + 0,iY - 1,iZ + 1); 
		values[12]= volume->voxel(iX + 0,iY + 0,iZ - 1); values[13] = volume->voxel(iX + 0,iY + 0,iZ + 0); values[14] = volume->voxel(iX + 0,iY + 0,iZ + 1); 
		values[15]= volume->voxel(iX + 0,iY + 1,iZ - 1); values[16] = volume->voxel(iX + 0,iY + 1,iZ + 0); values[17] = volume->voxel(iX + 0,iY + 1,iZ + 1); 
		
		values[18] = volume->voxel(iX + 1,iY - 1,iZ - 1); values[19] = volume->voxel(iX + 1,iY - 1,iZ + 0); values[20] = volume->voxel(iX + 1,iY - 1,iZ + 1); 
		values[21] = volume->voxel(iX + 1,iY + 0,iZ - 1); values[22] = volume->voxel(iX + 1,iY + 0,iZ + 0); values[23] = volume->voxel(iX + 1,iY + 0,iZ + 1); 
		values[24] = volume->voxel(iX + 1,iY + 1,iZ - 1); values[25] = volume->voxel(iX + 1,iY + 1,iZ + 0); values[26] = volume->voxel(iX + 1,iY + 1,iZ + 1); 	      
	      }
	      else
	      {
		for (int i = 0; i < 27; i++)
		{
		  values[i] = 0;		
		}		
	      }
	      
	      for (int i = 0; i < 27; i++)
	      {
		sum += values[i];		
	      }	      
	      average = sum / 27.f;        
	      _data->at(i).dataValues[1] = average;

	      
	      
	      //
	      // Standard deviation
	      float devsum = 0.f;
	      for(int i = 0; i < 27; i++)
	      {
		devsum += pow((values[i] - average),2);				
	      }      
	      float stdDeviation = sqrt(devsum/27);
	      // Compute the standard deviation
	      _data->at(i).dataValues[2] = stdDeviation;

	      
	      
	      
	      // Gradient magnitude
	      //	      
	      float gradientMagnitude = -1.f;
	      // Compute the gradient direction using either forward, central, or backward
	      // calculation and then take the magnitude (=length) of the vector.
	      // Hint:  tgt::vec3 is a class that can calculate the length for you
	          
	      float v1 = 0, v2 = 0, v3 = 0; 
	      
	      if (iX > 0 && iY > 0 && iZ > 0 && iX < (dimensions.x - 1) && iY < (dimensions.y - 1) && iZ < (dimensions.z - 1)) 
	      {
		v1 = volume->voxel(iX+1, iY, iZ)-volume->voxel(iX-1, iY, iZ); 
		v2 = volume->voxel(iX, iY+1, iZ)-volume->voxel(iX, iY-1, iZ); 
		v3 = volume->voxel(iX, iY, iZ+1)-volume->voxel(iX, iY, iZ+1); 
	      }
	      
	      gradientMagnitude = (1.f/2.f) * sqrt(pow(v1,2)+pow(v2,2)+pow(v3,2));
	      
	      _data->at(i).dataValues[3] = gradientMagnitude;
            }
        }
    }

    // sort the data by the voxel index for faster processing later
    std::sort(_data->begin(), _data->end(), sortByIndex);

    // And provide access to the data using the outport
    _outport.setData(_data, false); 
}

} // namespace
