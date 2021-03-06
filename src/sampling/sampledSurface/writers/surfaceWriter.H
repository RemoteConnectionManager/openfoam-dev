/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2012 OpenFOAM Foundation
     \\/     M anipulation  | Copyright (C) 2015-2018 OpenCFD Ltd.
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
    Foam::surfaceWriter

Description
    Base class for surface writers

SourceFiles
    surfaceWriter.C

\*---------------------------------------------------------------------------*/

#ifndef surfaceWriter_H
#define surfaceWriter_H

#include "typeInfo.H"
#include "autoPtr.H"
#include "fileName.H"
#include "meshedSurfRef.H"
#include "runTimeSelectionTables.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
                        Class surfaceWriter Declaration
\*---------------------------------------------------------------------------*/

class surfaceWriter
{
public:

    //- Runtime type information
    TypeName("surfaceWriter");

    // Declare run-time constructor selection table

        declareRunTimeSelectionTable
        (
            autoPtr,
            surfaceWriter,
            word,
            (),
            ()
        );

        declareRunTimeSelectionTable
        (
            autoPtr,
            surfaceWriter,
            wordDict,
            (
                const dictionary& options
            ),
            (options)
        );


    // Selectors

        //- Return a reference to the selected surfaceWriter
        static autoPtr<surfaceWriter> New(const word& writeType);

        //- Return a reference to the selected surfaceWriter
        //  Select with extra write option
        static autoPtr<surfaceWriter> New
        (
            const word& writeType,
            const dictionary& writeOptions
        );


    // Constructors

        //- Construct null
        surfaceWriter() = default;


    //- Destructor
    virtual ~surfaceWriter() = default;


    // Member Functions

        //- True if the surface format supports geometry in a separate file.
        //  False if geometry and field must be in a single file
        virtual bool separateGeometry() const
        {
            return false;
        }

        //- Trigger for geometry changes.
        //  \note this is a stop-gap solution until the writing infrastructure
        //  is improved.
        virtual void updateMesh
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName     //!< Name of surface
        ) const
        {}

        //- Write single surface geometry to file.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }

        //- Write scalarField for a single surface to file.
        //  One value per face or vertex.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const word& fieldName,          //!< Name of field
            const Field<scalar>& values,    //!< Field values to write
            const bool isNodeValues = false,//!< Values are per-vertex
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }

        //- Write vectorField for a single surface to file.
        //  One value per face or vertex.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const word& fieldName,          //!< Name of field
            const Field<vector>& values,    //!< Field values to write
            const bool isNodeValues = false,//!< Values are per-vertex
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }

        //- Write sphericalTensorField for a single surface to file.
        //  One value per face or vertex.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const word& fieldName,          //!< Name of field
            const Field<sphericalTensor>& values, //!< Field values to write
            const bool isNodeValues = false,//!< Values are per-vertex
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }

        //- Write symmTensorField for a single surface to file.
        //  One value per face or vertex.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const word& fieldName,          //!< Name of field
            const Field<symmTensor>& values,//!< Field values to write
            const bool isNodeValues = false,//!< Values are per-vertex
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }

        //- Write tensorField for a single surface to file.
        //  One value per face or vertex.
        virtual fileName write
        (
            const fileName& outputDir,      //!< output-dir
            const fileName& surfaceName,    //!< Name of surface
            const meshedSurf& surf,         //!< Surface geometry
            const word& fieldName,          //!< Name of field
            const Field<tensor>& values,    //!< Field values to write
            const bool isNodeValues = false,//!< Values are per-vertex
            const bool verbose = false      //!< Additional verbosity
        ) const
        {
            return fileName::null;
        }
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
