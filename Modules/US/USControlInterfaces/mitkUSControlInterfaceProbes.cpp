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

#include "mitkUSControlInterfaceProbes.h"
#include "mitkUSDevice.h"

mitk::USControlInterfaceProbes::USControlInterfaceProbes(itk::SmartPointer<USDevice> device)
  : mitk::USAbstractControlInterface(device)
{
}

mitk::USControlInterfaceProbes::~USControlInterfaceProbes()
{
}