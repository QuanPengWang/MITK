/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#include "mitkImageLiveWireContourModelFilter.h"


#include <itkImageRegionIterator.h>
#include <itkCastImageFilter.h>
#include <itkGradientMagnitudeImageFilter.h>


mitk::ImageLiveWireContourModelFilter::ImageLiveWireContourModelFilter()
{
  OutputType::Pointer output = dynamic_cast<OutputType*> ( this->MakeOutput( 0 ).GetPointer() );
  this->SetNumberOfRequiredInputs(1);
  this->SetNumberOfOutputs( 1 );
  this->SetNthOutput(0, output.GetPointer());
  m_CostFunction = CostFunctionType::New();
  m_ShortestPathFilter = ShortestPathImageFilterType::New();
  m_ShortestPathFilter->SetCostFunction(m_CostFunction);
  m_UseDynamicCostTransferForNextUpdate = false;
}

mitk::ImageLiveWireContourModelFilter::~ImageLiveWireContourModelFilter()
{

}


mitk::ImageLiveWireContourModelFilter::OutputType* mitk::ImageLiveWireContourModelFilter::GetOutput()
{
  return Superclass::GetOutput();
}

void mitk::ImageLiveWireContourModelFilter::SetInput ( const mitk::ImageLiveWireContourModelFilter::InputType* input )
{
  this->SetInput( 0, input );
}

void mitk::ImageLiveWireContourModelFilter::SetInput ( unsigned int idx, const mitk::ImageLiveWireContourModelFilter::InputType* input )
{
  if ( idx + 1 > this->GetNumberOfInputs() )
  {
    this->SetNumberOfRequiredInputs(idx + 1);
  }
  if ( input != static_cast<InputType*> ( this->ProcessObject::GetInput ( idx ) ) )
  {
    this->ProcessObject::SetNthInput ( idx, const_cast<InputType*> ( input ) );
    this->Modified();
  }
}



const mitk::ImageLiveWireContourModelFilter::InputType* mitk::ImageLiveWireContourModelFilter::GetInput( void )
{
  if (this->GetNumberOfInputs() < 1)
    return NULL;
  return static_cast<const mitk::ImageLiveWireContourModelFilter::InputType*>(this->ProcessObject::GetInput(0));
}


const mitk::ImageLiveWireContourModelFilter::InputType* mitk::ImageLiveWireContourModelFilter::GetInput( unsigned int idx )
{
  if (this->GetNumberOfInputs() < 1)
    return NULL;
  return static_cast<const mitk::ImageLiveWireContourModelFilter::InputType*>(this->ProcessObject::GetInput(idx));
}


void mitk::ImageLiveWireContourModelFilter::GenerateData()
{
  mitk::Image::ConstPointer input = dynamic_cast<const mitk::Image*>(this->GetInput());

  if(!input)
  {
    MITK_ERROR << "No input available.";
    itkExceptionMacro("mitk::ImageToLiveWireContourFilter: No input available. Please set the input!");
    return;
  }

  if( input->GetDimension() != 2 )
  {
    MITK_ERROR << "Filter is only working on 2D images.";
    itkExceptionMacro("mitk::ImageToLiveWireContourFilter: Filter is only working on 2D images.. Please make sure that the input is 2D!");
    return;
  }


  input->GetGeometry()->WorldToIndex(m_StartPoint, m_StartPointInIndex);
  input->GetGeometry()->WorldToIndex(m_EndPoint, m_EndPointInIndex);

  //only start calculating if both indices are inside image geometry
  if( input->GetGeometry()->IsIndexInside(this->m_StartPointInIndex) && input->GetGeometry()->IsIndexInside(this->m_EndPointInIndex) )
  {
    AccessFixedDimensionByItk(input, ItkProcessImage, 2);
  }
}



template<typename TPixel, unsigned int VImageDimension>
void mitk::ImageLiveWireContourModelFilter::ItkProcessImage (itk::Image<TPixel, VImageDimension>* inputImage)
{
  typedef itk::Image< TPixel, VImageDimension >   InputImageType;
  typedef InputImageType::IndexType               IndexType;


  /* compute the requested region for itk filters */

  typename IndexType startPoint, endPoint;

  startPoint[0] = m_StartPointInIndex[0];
  startPoint[1] = m_StartPointInIndex[1];

  endPoint[0] = m_EndPointInIndex[0];
  endPoint[1] = m_EndPointInIndex[1];

  //minimum value in each direction for startRegion
  typename IndexType startRegion;
  startRegion[0] = startPoint[0] < endPoint[0] ? startPoint[0] : endPoint[0];
  startRegion[1] = startPoint[1] < endPoint[1] ? startPoint[1] : endPoint[1];

  //maximum value in each direction for size
  typename InputImageType::SizeType size;
  size[0] = abs( startPoint[0] - endPoint[0] );
  size[1] = abs( startPoint[1] - endPoint[1] );


  typename CostFunctionType::RegionType region;
  region.SetSize( size );
  region.SetIndex( startRegion );
  /*---------------------------------------------*/

  //inputImage->SetRequestedRegion(region);

  typedef itk::CastImageFilter< InputImageType, FloatImageType > CastFilterType;
  CastFilterType::Pointer castFilter = CastFilterType::New();
  castFilter->SetInput(inputImage);
  castFilter->Update();
  /* extracts features from image and calculates costs */
  m_CostFunction->SetImage(castFilter->GetOutput());
  m_CostFunction->SetStartIndex(startPoint);
  m_CostFunction->SetEndIndex(endPoint);
  m_CostFunction->SetRequestedRegion(region);
  /*---------------------------------------------*/


  /* calculate shortest path between start and end point */
  m_ShortestPathFilter->SetFullNeighborsMode(true);
  m_ShortestPathFilter->SetInput(castFilter->GetOutput());
  m_ShortestPathFilter->SetMakeOutputImage(false);  

  //m_ShortestPathFilter->SetCalcAllDistances(true);
  m_ShortestPathFilter->SetStartIndex(startPoint);
  m_ShortestPathFilter->SetEndIndex(endPoint);


  m_ShortestPathFilter->Update();

  /*---------------------------------------------*/


  /* construct contour from path image */
  //get the shortest path as vector
  typename std::vector< ShortestPathImageFilterType::IndexType> shortestPath = m_ShortestPathFilter->GetVectorPath();

  //fill the output contour with controll points from the path
  OutputType::Pointer output = dynamic_cast<OutputType*> ( this->MakeOutput( 0 ).GetPointer() );
  this->SetNthOutput(0, output.GetPointer());

  mitk::Image::ConstPointer input = dynamic_cast<const mitk::Image*>(this->GetInput());

  typename std::vector< ShortestPathImageFilterType::IndexType>::iterator pathIterator = shortestPath.begin();

  while(pathIterator != shortestPath.end())
  {
    mitk::Point3D currentPoint;
    currentPoint[0] = (*pathIterator)[0];
    currentPoint[1] = (*pathIterator)[1];
    currentPoint[2] = 0;


    input->GetGeometry()->IndexToWorld(currentPoint, currentPoint);
    output->AddVertex(currentPoint);

    pathIterator++;
  }
  /*---------------------------------------------*/

  /*++++++++++ create dynamic cost transfer map ++++++++++*/ 
  if(this->m_UseDynamicCostTransferForNextUpdate)
  {

    /*+++ filter image gradient magnitude +++*/
    typedef  itk::GradientMagnitudeImageFilter< itk::Image<TPixel, VImageDimension>,  itk::Image<TPixel, VImageDimension>> GradientMagnitudeFilterType;
    typename GradientMagnitudeFilterType::Pointer gradientFilter = GradientMagnitudeFilterType::New();
    gradientFilter->SetInput(inputImage);
    gradientFilter->Update();
    itk::Image<TPixel, VImageDimension>::Pointer gradientMagnImage = gradientFilter->GetOutput();

    //get the path
    typename std::vector< ShortestPathImageFilterType::IndexType> shortestPath = m_ShortestPathFilter->GetVectorPath();

    //iterator of path
    typename std::vector< ShortestPathImageFilterType::IndexType>::iterator pathIterator = shortestPath.begin();

    std::map< int, int> histogram;

    //create histogram within path
    while(pathIterator != shortestPath.end())
    {
      //count pixel values
      histogram[ static_cast<int>( gradientMagnImage->GetPixel((*pathIterator)) ) ] += 1;

      pathIterator++;
    }


    //get max of histogramm
    int max = 0;
    std::map< int, int>::iterator it = histogram.begin();
    while( it != histogram.end())
    {
      if((*it).second > max)
        max = (*it).second;
      it++;
    }


    //invert map according to 1-x/max
    it = histogram.begin();
    while( it != histogram.end())
    {
      (*it).second = 1 - (*it).second / max;
      it++;
    }

    this->m_CostFunction->SetDynamicCostMap(histogram);
    this->m_CostFunction->SetCostMapMAX(max);
  }
}