/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2017 OpenFOAM Foundation
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

\*---------------------------------------------------------------------------*/

#include "triSurfaceMesh.H"
#include "Random.H"
#include "addToRunTimeSelectionTable.H"
#include "edgeHashes.H"
#include "triSurfaceFields.H"
#include "Time.H"
#include "PatchTools.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(triSurfaceMesh, 0);
    addToRunTimeSelectionTable(searchableSurface, triSurfaceMesh, dict);
}

Foam::word Foam::triSurfaceMesh::meshSubDir = "triSurface";


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

Foam::fileName Foam::triSurfaceMesh::checkFile
(
    const IOobject& io,
    const bool isGlobal
)
{
    const fileName fName
    (
        isGlobal
      ? io.globalFilePath(typeName)
      : io.localFilePath(typeName)
    );
    if (fName.empty())
    {
        FatalErrorInFunction
            << "Cannot find triSurfaceMesh starting from "
            << io.objectPath() << exit(FatalError);
    }

    return fName;
}


Foam::fileName Foam::triSurfaceMesh::relativeFilePath
(
    const regIOobject& io,
    const fileName& f,
    const bool isGlobal
)
{
    fileName fName(f);
    fName.expand();
    if (!fName.isAbsolute())
    {
        // Is the specified file:
        // - local to the cwd?
        // - local to the case dir?
        // - or just another name?
        fName = fileHandler().filePath
        (
            isGlobal,
            IOobject(io, fName),
            word::null
        );
    }
    return fName;
}

Foam::fileName Foam::triSurfaceMesh::checkFile
(
    const regIOobject& io,
    const dictionary& dict,
    const bool isGlobal
)
{
    fileName fName;
    if (dict.readIfPresent("file", fName, keyType::LITERAL))
    {
        const fileName rawFName(fName);

        fName = relativeFilePath(io, fName, isGlobal);

        if (!exists(fName))
        {
            FatalErrorInFunction
                << "Cannot find triSurfaceMesh " << rawFName
                << " starting from " << io.objectPath()
                << exit(FatalError);
        }
    }
    else
    {
        fName =
        (
            isGlobal
          ? io.globalFilePath(typeName)
          : io.localFilePath(typeName)
        );

        if (!exists(fName))
        {
            FatalErrorInFunction
                << "Cannot find triSurfaceMesh starting from "
                << io.objectPath() << exit(FatalError);
        }
    }

    return fName;
}


bool Foam::triSurfaceMesh::addFaceToEdge
(
    const edge& e,
    EdgeMap<label>& facesPerEdge
)
{
    label& count = facesPerEdge(e, 0);  // lookup or new entry
    if (count == 2)
    {
        return false;
    }

    ++count;
    return true;
}


bool Foam::triSurfaceMesh::isSurfaceClosed() const
{
    const pointField& pts = triSurface::points();

    // Construct pointFaces. Let's hope surface has compact point
    // numbering ...
    labelListList pointFaces;
    invertManyToMany(pts.size(), *this, pointFaces);

    // Loop over all faces surrounding point. Count edges emanating from point.
    // Every edge should be used by two faces exactly.
    // To prevent doing work twice per edge only look at edges to higher
    // point
    EdgeMap<label> facesPerEdge(128);
    forAll(pointFaces, pointi)
    {
        const labelList& pFaces = pointFaces[pointi];

        facesPerEdge.clear();
        for (const label facei : pFaces)
        {
            const triSurface::FaceType& f = triSurface::operator[](facei);
            const label fp = f.find(pointi);

            // Something weird: if I expand the code of addFaceToEdge in both
            // below instances it gives a segmentation violation on some
            // surfaces. Compiler (4.3.2) problem?


            // Forward edge
            const label nextPointi = f[f.fcIndex(fp)];

            if (nextPointi > pointi)
            {
                bool okFace = addFaceToEdge
                (
                    edge(pointi, nextPointi),
                    facesPerEdge
                );

                if (!okFace)
                {
                    return false;
                }
            }

            // Reverse edge
            const label prevPointi = f[f.rcIndex(fp)];

            if (prevPointi > pointi)
            {
                bool okFace = addFaceToEdge
                (
                    edge(pointi, prevPointi),
                    facesPerEdge
                );

                if (!okFace)
                {
                    return false;
                }
            }
        }

        // Check for any edges used only once.
        forAllConstIters(facesPerEdge, iter)
        {
            if (iter.object() != 2)
            {
                return false;
            }
        }
    }

    return true;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::triSurfaceMesh::triSurfaceMesh(const IOobject& io, const triSurface& s)
:
    searchableSurface(io),
    objectRegistry
    (
        IOobject
        (
            io.name(),
            io.instance(),
            io.local(),
            io.db(),
            io.readOpt(),
            io.writeOpt(),
            false       // searchableSurface already registered under name
        )
    ),
    triSurface(s),
    triSurfaceRegionSearch(static_cast<const triSurface&>(*this)),
    minQuality_(-1),
    surfaceClosed_(-1),
    outsideVolType_(volumeType::UNKNOWN)
{
    const pointField& pts = triSurface::points();

    bounds() = boundBox(pts, false);
}


Foam::triSurfaceMesh::triSurfaceMesh(const IOobject& io)
:
    // Find instance for triSurfaceMesh
    searchableSurface(io),
    // Reused found instance in objectRegistry
    objectRegistry
    (
        IOobject
        (
            io.name(),
            searchableSurface::instance(),
            io.local(),
            io.db(),
            io.readOpt(),
            io.writeOpt(),
            false       // searchableSurface already registered under name
        )
    ),
    triSurface(checkFile(static_cast<const searchableSurface&>(*this), true)),
    triSurfaceRegionSearch(static_cast<const triSurface&>(*this)),
    minQuality_(-1),
    surfaceClosed_(-1),
    outsideVolType_(volumeType::UNKNOWN)
{
    const pointField& pts = triSurface::points();

    bounds() = boundBox(pts, false);
}


Foam::triSurfaceMesh::triSurfaceMesh
(
    const IOobject& io,
    const dictionary& dict
)
:
    searchableSurface(io),
    // Reused found instance in objectRegistry
    objectRegistry
    (
        IOobject
        (
            io.name(),
            searchableSurface::instance(),
            io.local(),
            io.db(),
            io.readOpt(),
            io.writeOpt(),
            false       // searchableSurface already registered under name
        )
    ),
    triSurface
    (
        checkFile(static_cast<const searchableSurface&>(*this), dict, true)
    ),
    triSurfaceRegionSearch(static_cast<const triSurface&>(*this), dict),
    minQuality_(-1),
    surfaceClosed_(-1),
    outsideVolType_(volumeType::UNKNOWN)
{
    // Reading from supplied file name instead of objectPath/filePath
    if (dict.readIfPresent("file", fName_, keyType::LITERAL))
    {
        fName_ = relativeFilePath
        (
            static_cast<const searchableSurface&>(*this),
            fName_,
            true
        );
    }

    scalar scaleFactor = 0;

    // Allow rescaling of the surface points
    // eg, CAD geometries are often done in millimeters
    if (dict.readIfPresent("scale", scaleFactor) && scaleFactor > 0)
    {
        Info<< searchableSurface::name() << " : using scale " << scaleFactor
            << endl;
        triSurface::scalePoints(scaleFactor);
    }

    const pointField& pts = triSurface::points();

    bounds() = boundBox(pts, false);

    // Have optional minimum quality for normal calculation
    if (dict.readIfPresent("minQuality", minQuality_) && minQuality_ > 0)
    {
        Info<< searchableSurface::name()
            << " : ignoring triangles with quality < "
            << minQuality_ << " for normals calculation." << endl;
    }
}


Foam::triSurfaceMesh::triSurfaceMesh(const IOobject& io, const bool isGlobal)
:
    // Find instance for triSurfaceMesh
    searchableSurface(io),
    // Reused found instance in objectRegistry
    objectRegistry
    (
        IOobject
        (
            io.name(),
            searchableSurface::instance(),
            io.local(),
            io.db(),
            io.readOpt(),
            io.writeOpt(),
            false       // searchableSurface already registered under name
        )
    ),
    triSurface
    (
        checkFile(static_cast<const searchableSurface&>(*this), isGlobal)
    ),
    triSurfaceRegionSearch(static_cast<const triSurface&>(*this)),
    minQuality_(-1),
    surfaceClosed_(-1),
    outsideVolType_(volumeType::UNKNOWN)
{
    const pointField& pts = triSurface::points();

    bounds() = boundBox(pts, isGlobal);
}


Foam::triSurfaceMesh::triSurfaceMesh
(
    const IOobject& io,
    const dictionary& dict,
    const bool isGlobal
)
:
    searchableSurface(io),
    // Reused found instance in objectRegistry
    objectRegistry
    (
        IOobject
        (
            io.name(),
            searchableSurface::instance(),
            io.local(),
            io.db(),
            io.readOpt(),
            io.writeOpt(),
            false       // searchableSurface already registered under name
        )
    ),
    triSurface
    (
        checkFile(static_cast<const searchableSurface&>(*this), dict, isGlobal)
    ),
    triSurfaceRegionSearch(static_cast<const triSurface&>(*this), dict),
    minQuality_(-1),
    surfaceClosed_(-1),
    outsideVolType_(volumeType::UNKNOWN)
{
    // Reading from supplied file name instead of objectPath/filePath
    if (dict.readIfPresent("file", fName_, keyType::LITERAL))
    {
        fName_ = relativeFilePath
        (
            static_cast<const searchableSurface&>(*this),
            fName_,
            isGlobal
        );
    }

    scalar scaleFactor = 0;

    // Allow rescaling of the surface points
    // eg, CAD geometries are often done in millimeters
    if (dict.readIfPresent("scale", scaleFactor) && scaleFactor > 0)
    {
        Info<< searchableSurface::name() << " : using scale " << scaleFactor
            << endl;
        triSurface::scalePoints(scaleFactor);
    }

    const pointField& pts = triSurface::points();

    bounds() = boundBox(pts, isGlobal);

    // Have optional minimum quality for normal calculation
    if (dict.readIfPresent("minQuality", minQuality_) && minQuality_ > 0)
    {
        Info<< searchableSurface::name()
            << " : ignoring triangles with quality < "
            << minQuality_ << " for normals calculation." << endl;
    }
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::triSurfaceMesh::~triSurfaceMesh()
{
    clearOut();
}


void Foam::triSurfaceMesh::clearOut()
{
    outsideVolType_ = volumeType::UNKNOWN;
    triSurfaceRegionSearch::clearOut();
    edgeTree_.clear();
    triSurface::clearOut();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

Foam::tmp<Foam::pointField> Foam::triSurfaceMesh::coordinates() const
{
    auto tpts = tmp<pointField>::New(8);
    auto& pts = tpts.ref();

    // Use copy to calculate face centres so they don't get stored
    pts = PrimitivePatch<triSurface::FaceType, SubList, const pointField&>
    (
        SubList<triSurface::FaceType>(*this, triSurface::size()),
        triSurface::points()
    ).faceCentres();

    return tpts;
}


void Foam::triSurfaceMesh::boundingSpheres
(
    pointField& centres,
    scalarField& radiusSqr
) const
{
    centres = coordinates();
    radiusSqr.setSize(size());
    radiusSqr = 0.0;

    const pointField& pts = triSurface::points();

    forAll(*this, facei)
    {
        const labelledTri& f = triSurface::operator[](facei);
        const point& fc = centres[facei];
        for (const label pointi : f)
        {
            const point& pt = pts[pointi];
            radiusSqr[facei] = max(radiusSqr[facei], Foam::magSqr(fc-pt));
        }
    }

    // Add a bit to make sure all points are tested inside
    radiusSqr += Foam::sqr(SMALL);
}


Foam::tmp<Foam::pointField> Foam::triSurfaceMesh::points() const
{
    return triSurface::points();
}


bool Foam::triSurfaceMesh::overlaps(const boundBox& bb) const
{
     const indexedOctree<treeDataTriSurface>& octree = tree();

     labelList indices = octree.findBox(treeBoundBox(bb));

     return !indices.empty();
}


void Foam::triSurfaceMesh::movePoints(const pointField& newPoints)
{
    outsideVolType_ = volumeType::UNKNOWN;

    // Update local information (instance, event number)
    searchableSurface::instance() = objectRegistry::time().timeName();
    objectRegistry::instance() = searchableSurface::instance();

    const label event = getEvent();
    searchableSurface::eventNo() = event;
    objectRegistry::eventNo() = searchableSurface::eventNo();

    // Clear additional addressing
    triSurfaceRegionSearch::clearOut();
    edgeTree_.clear();
    triSurface::movePoints(newPoints);

    bounds() = boundBox(triSurface::points(), false);
}


const Foam::indexedOctree<Foam::treeDataEdge>&
Foam::triSurfaceMesh::edgeTree() const
{
    if (edgeTree_.empty())
    {
        // Boundary edges
        labelList bEdges
        (
            identity(nEdges() - nInternalEdges(), nInternalEdges())
        );

        treeBoundBox bb(Zero, Zero);

        if (bEdges.size())
        {
            label nPoints;
            PatchTools::calcBounds
            (
                *this,
                bb,
                nPoints
            );

            // Random number generator. Bit dodgy since not exactly random ;-)
            Random rndGen(65431);

            // Slightly extended bb. Slightly off-centred just so on symmetric
            // geometry there are less face/edge aligned items.

            bb = bb.extend(rndGen, 1e-4);
            bb.min() -= point(ROOTVSMALL, ROOTVSMALL, ROOTVSMALL);
            bb.max() += point(ROOTVSMALL, ROOTVSMALL, ROOTVSMALL);
        }

        scalar oldTol = indexedOctree<treeDataEdge>::perturbTol();
        indexedOctree<treeDataEdge>::perturbTol() = tolerance();

        edgeTree_.reset
        (
            new indexedOctree<treeDataEdge>
            (
                treeDataEdge
                (
                    false,          // cachebb
                    edges(),        // edges
                    localPoints(),  // points
                    bEdges          // selected edges
                ),
                bb,                 // bb
                maxTreeDepth(),     // maxLevel
                10,                 // leafsize
                3.0                 // duplicity
            )
        );

        indexedOctree<treeDataEdge>::perturbTol() = oldTol;
    }

    return *edgeTree_;
}


const Foam::wordList& Foam::triSurfaceMesh::regions() const
{
    if (regions_.empty())
    {
        regions_.setSize(patches().size());
        forAll(regions_, regioni)
        {
            regions_[regioni] = patches()[regioni].name();
        }
    }
    return regions_;
}


bool Foam::triSurfaceMesh::hasVolumeType() const
{
    if (surfaceClosed_ == -1)
    {
        if (isSurfaceClosed())
        {
            surfaceClosed_ = 1;
        }
        else
        {
            surfaceClosed_ = 0;
        }
    }

    return surfaceClosed_ == 1;
}


void Foam::triSurfaceMesh::findNearest
(
    const pointField& samples,
    const scalarField& nearestDistSqr,
    List<pointIndexHit>& info
) const
{
    triSurfaceSearch::findNearest(samples, nearestDistSqr, info);
}


void Foam::triSurfaceMesh::findNearest
(
    const pointField& samples,
    const scalarField& nearestDistSqr,
    const labelList& regionIndices,
    List<pointIndexHit>& info
) const
{
    triSurfaceRegionSearch::findNearest
    (
        samples,
        nearestDistSqr,
        regionIndices,
        info
    );
}


void Foam::triSurfaceMesh::findLine
(
    const pointField& start,
    const pointField& end,
    List<pointIndexHit>& info
) const
{
    triSurfaceSearch::findLine(start, end, info);
}


void Foam::triSurfaceMesh::findLineAny
(
    const pointField& start,
    const pointField& end,
    List<pointIndexHit>& info
) const
{
    triSurfaceSearch::findLineAny(start, end, info);
}


void Foam::triSurfaceMesh::findLineAll
(
    const pointField& start,
    const pointField& end,
    List<List<pointIndexHit>>& info
) const
{
    triSurfaceSearch::findLineAll(start, end, info);
}


void Foam::triSurfaceMesh::getRegion
(
    const List<pointIndexHit>& info,
    labelList& region
) const
{
    region.setSize(info.size());
    forAll(info, i)
    {
        if (info[i].hit())
        {
            region[i] = triSurface::operator[](info[i].index()).region();
        }
        else
        {
            region[i] = -1;
        }
    }
}


void Foam::triSurfaceMesh::getNormal
(
    const List<pointIndexHit>& info,
    vectorField& normal
) const
{
    const triSurface& s = *this;
    const pointField& pts = s.points();

    normal.setSize(info.size());

    if (minQuality_ >= 0)
    {
        // Make sure we don't use triangles with low quality since
        // normal is not reliable.

        const labelListList& faceFaces = s.faceFaces();

        forAll(info, i)
        {
            if (info[i].hit())
            {
                const label facei = info[i].index();
                normal[i] = s[facei].unitNormal(pts);

                scalar qual = s[facei].tri(pts).quality();

                if (qual < minQuality_)
                {
                    // Search neighbouring triangles
                    const labelList& fFaces = faceFaces[facei];

                    for (const label nbri : fFaces)
                    {
                        scalar nbrQual = s[nbri].tri(pts).quality();
                        if (nbrQual > qual)
                        {
                            qual = nbrQual;
                            normal[i] = s[nbri].unitNormal(pts);
                        }
                    }
                }
            }
            else
            {
                // Set to what?
                normal[i] = Zero;
            }
        }
    }
    else
    {
        forAll(info, i)
        {
            if (info[i].hit())
            {
                const label facei = info[i].index();

                // Uncached
                normal[i] = s[facei].unitNormal(pts);
            }
            else
            {
                // Set to what?
                normal[i] = Zero;
            }
        }
    }
}


void Foam::triSurfaceMesh::setField(const labelList& values)
{
    auto* fldPtr = getObjectPtr<triSurfaceLabelField>("values");

    if (fldPtr)
    {
        (*fldPtr).field() = values;
    }
    else
    {
        fldPtr = new triSurfaceLabelField
        (
            IOobject
            (
                "values",
                objectRegistry::time().timeName(),  // instance
                meshSubDir,                         // local
                *this,
                IOobject::NO_READ,
                IOobject::AUTO_WRITE
            ),
            *this,
            dimless,
            labelField(values)
        );

        // Store field on triMesh
        fldPtr->store();
    }
}


void Foam::triSurfaceMesh::getField
(
    const List<pointIndexHit>& info,
    labelList& values
) const
{
    const auto* fldPtr = getObjectPtr<triSurfaceLabelField>("values");

    if (fldPtr)
    {
        const auto& fld = *fldPtr;

        values.setSize(info.size());

        forAll(info, i)
        {
            if (info[i].hit())
            {
                values[i] = fld[info[i].index()];
            }
        }
    }
}


void Foam::triSurfaceMesh::getVolumeType
(
    const pointField& points,
    List<volumeType>& volType
) const
{
    const scalar oldTol = indexedOctree<treeDataTriSurface>::perturbTol();
    indexedOctree<treeDataTriSurface>::perturbTol() = tolerance();

    volType.setSize(points.size());

    forAll(points, pointi)
    {
        const point& pt = points[pointi];

        if (tree().bb().contains(pt))
        {
            // Use cached volume type per each tree node
            volType[pointi] = tree().getVolumeType(pt);
        }
        else if (hasVolumeType())
        {
            // Precalculate and cache value for this outside point
            if (outsideVolType_ == volumeType::UNKNOWN)
            {
                outsideVolType_ = tree().shapes().getVolumeType(tree(), pt);
            }
            volType[pointi] = outsideVolType_;
        }
        else
        {
            // Have to calculate directly as outside the octree
            volType[pointi] = tree().shapes().getVolumeType(tree(), pt);
        }
    }

    indexedOctree<treeDataTriSurface>::perturbTol() = oldTol;
}


bool Foam::triSurfaceMesh::writeObject
(
    IOstream::streamFormat fmt,
    IOstream::versionNumber ver,
    IOstream::compressionType cmp,
    const bool valid
) const
{
    const Time& runTime = searchableSurface::time();
    const fileName& instance = searchableSurface::instance();

    if
    (
        instance != runTime.timeName()
     && instance != runTime.system()
     && instance != runTime.caseSystem()
     && instance != runTime.constant()
     && instance != runTime.caseConstant()
    )
    {
        const_cast<triSurfaceMesh&>(*this).searchableSurface::instance() =
            runTime.timeName();
        const_cast<triSurfaceMesh&>(*this).objectRegistry::instance() =
            runTime.timeName();
    }

    fileName fullPath;
    if (fName_.size())
    {
        // Override file name

        fullPath = fName_;

        fullPath.expand();
        if (!fullPath.isAbsolute())
        {
            // Add directory from regIOobject
            fullPath = searchableSurface::objectPath().path()/fullPath;
        }
    }
    else
    {
        fullPath = searchableSurface::objectPath();
    }

    if (!mkDir(fullPath.path()))
    {
        return false;
    }

    triSurface::write(fullPath);

    if (!isFile(fullPath))
    {
        return false;
    }

    return true;
}


// ************************************************************************* //
