/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Class
    pqPVblockMeshReaderPanel

Description
    GUI modifications for the ParaView reader panel

    A custom panel for the PVblockMeshReader.

SourceFiles
    pqPVblockMeshReaderPanel.cxx

\*---------------------------------------------------------------------------*/
#ifndef pqPVblockMeshReaderPanel_h
#define pqPVblockMeshReaderPanel_h

#include "pqAutoGeneratedObjectPanel.h"

// Forward declaration of QT classes

class QCheckBox;
class QLineEdit;
class QTimer;
class QToolButton;

// Forward declaration of ParaView classes
class vtkSMSourceProxy;


/*---------------------------------------------------------------------------*\
                 Class pqPVblockMeshReaderPanel Declaration
\*---------------------------------------------------------------------------*/

class pqPVblockMeshReaderPanel
:
    public pqAutoGeneratedObjectPanel
{
    // Private data
    Q_OBJECT;
    typedef pqAutoGeneratedObjectPanel Superclass;

    //- Show Point Numbers checkbox
    QCheckBox* ShowPointNumbers_;

protected slots:

    void ShowPointNumbersToggled();
    void RefreshPressed();


public:

    // Constructors

        //- Construct from components
        pqPVblockMeshReaderPanel(pqProxy*, QWidget*);
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
