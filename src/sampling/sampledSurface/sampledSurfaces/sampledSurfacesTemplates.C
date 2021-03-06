/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
     \\/     M anipulation  | Copyright (C) 2015-2016 OpenCFD Ltd.
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

\*---------------------------------------------------------------------------*/

#include "sampledSurfaces.H"
#include "volFields.H"
#include "surfaceFields.H"
#include "ListListOps.H"
#include "stringListOps.H"

// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

template<class Type>
void Foam::sampledSurfaces::writeSurface
(
    const Field<Type>& values,
    const label surfi,
    const word& fieldName,
    const fileName& outputDir
)
{
    const sampledSurface& s = operator[](surfi);

    if (changedGeom_[surfi])
    {
        // Trigger any changes
        formatter_->updateMesh(outputDir, s.name());
        changedGeom_[surfi] = false;
    }

    if (Pstream::parRun())
    {
        // Collect values from all processors
        List<Field<Type>> gatheredValues(Pstream::nProcs());
        gatheredValues[Pstream::myProcNo()] = values;
        Pstream::gatherList(gatheredValues);

        fileName sampleFile;
        if (Pstream::master())
        {
            // Combine values into single field
            Field<Type> allValues
            (
                ListListOps::combine<Field<Type>>
                (
                    gatheredValues,
                    accessOp<Field<Type>>()
                )
            );

            // Renumber (point data) to correspond to merged points
            if (mergedList_[surfi].pointsMap().size() == allValues.size())
            {
                inplaceReorder(mergedList_[surfi].pointsMap(), allValues);
                allValues.setSize(mergedList_[surfi].points().size());
            }

            // Write to time directory under outputPath_
            // skip surface without faces (eg, a failed cut-plane)
            if (mergedList_[surfi].size())
            {
                sampleFile = formatter_->write
                (
                    outputDir,
                    s.name(),
                    mergedList_[surfi],
                    fieldName,
                    allValues,
                    s.interpolate()
                );
            }
        }

        Pstream::scatter(sampleFile);
        if (sampleFile.size())
        {
            // Case-local file name with "<case>" to make relocatable

            dictionary propsDict;
            propsDict.add
            (
                "file",
                time_.relativePath(sampleFile, true)
            );
            setProperty(fieldName, propsDict);
        }
    }
    else
    {
        // Write to time directory under outputPath_
        // skip surface without faces (eg, a failed cut-plane)
        if (s.faces().size())
        {
            fileName fName = formatter_->write
            (
                outputDir,
                s.name(),
                s,
                fieldName,
                values,
                s.interpolate()
            );

            // Case-local file name with "<case>" to make relocatable

            dictionary propsDict;
            propsDict.add
            (
                "file",
                time_.relativePath(fName, true)
            );
            setProperty(fieldName, propsDict);
        }
    }
}


template<class Type>
void Foam::sampledSurfaces::sampleAndWrite
(
    const GeometricField<Type, fvPatchField, volMesh>& vField
)
{
    // sampler/interpolator for this field
    autoPtr<interpolation<Type>> interpPtr;

    const word& fieldName = vField.name();
    const fileName outputDir = outputPath_/vField.time().timeName();

    forAll(*this, surfi)
    {
        const sampledSurface& s = operator[](surfi);

        Field<Type> values;

        if (s.interpolate())
        {
            if (interpPtr.empty())
            {
                interpPtr = interpolation<Type>::New
                (
                    sampleNodeScheme_,
                    vField
                );
            }

            values = s.interpolate(*interpPtr);
        }
        else
        {
            if (interpPtr.empty())
            {
                interpPtr = interpolation<Type>::New
                (
                    sampleFaceScheme_,
                    vField
                );
            }

            values = s.sample(*interpPtr);
        }

        writeSurface<Type>(values, surfi, fieldName, outputDir);
    }
}


template<class Type>
void Foam::sampledSurfaces::sampleAndWrite
(
    const GeometricField<Type, fvsPatchField, surfaceMesh>& sField
)
{
    const word& fieldName = sField.name();
    const fileName outputDir = outputPath_/sField.time().timeName();

    forAll(*this, surfi)
    {
        const sampledSurface& s = operator[](surfi);
        Field<Type> values(s.sample(sField));
        writeSurface<Type>(values, surfi, fieldName, outputDir);
    }
}


template<class GeoField>
void Foam::sampledSurfaces::sampleAndWrite(const IOobjectList& objects)
{
    wordList fieldNames;
    if (loadFromFiles_)
    {
        fieldNames = objects.sortedNames(GeoField::typeName, fieldSelection_);
    }
    else
    {
        fieldNames = mesh_.thisDb().sortedNames<GeoField>(fieldSelection_);

        writeOriginalIds();
    }

    for (const word& fieldName : fieldNames)
    {
        if (verbose_)
        {
            Info<< "sampleAndWrite: " << fieldName << endl;
        }

        if (loadFromFiles_)
        {
            const GeoField fld
            (
                IOobject
                (
                    fieldName,
                    time_.timeName(),
                    mesh_,
                    IOobject::MUST_READ
                ),
                mesh_
            );

            sampleAndWrite(fld);
        }
        else
        {
            sampleAndWrite
            (
                mesh_.thisDb().lookupObject<GeoField>(fieldName)
            );
        }
    }
}


// ************************************************************************* //
