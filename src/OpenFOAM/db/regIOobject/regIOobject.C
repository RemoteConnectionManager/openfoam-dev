/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2017 OpenFOAM Foundation
     \\/     M anipulation  | Copyright (C) 2018 OpenCFD Ltd.
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

#include "regIOobject.H"
#include "Time.H"
#include "polyMesh.H"
#include "registerSwitch.H"
#include "fileOperation.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(regIOobject, 0);
}

float Foam::regIOobject::fileModificationSkew
(
    Foam::debug::floatOptimisationSwitch("fileModificationSkew", 30)
);
registerOptSwitch
(
    "fileModificationSkew",
    float,
    Foam::regIOobject::fileModificationSkew
);


bool Foam::regIOobject::masterOnlyReading = false;


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::regIOobject::regIOobject(const IOobject& io, const bool isTime)
:
    IOobject(io),
    registered_(false),
    ownedByRegistry_(false),
    watchIndices_(),
    eventNo_                // Do not get event for top level Time database
    (
        isTime
      ? 0
      : db().getEvent()
    )
{
    // Register with objectRegistry if requested
    if (registerObject())
    {
        checkIn();
    }
}


Foam::regIOobject::regIOobject(const regIOobject& rio)
:
    IOobject(rio),
    registered_(false),
    ownedByRegistry_(false),
    watchIndices_(rio.watchIndices_),
    eventNo_(db().getEvent()),
    isPtr_(nullptr)
{
    // Do not register copy with objectRegistry
}


Foam::regIOobject::regIOobject(const regIOobject& rio, bool registerCopy)
:
    IOobject(rio),
    registered_(false),
    ownedByRegistry_(false),
    watchIndices_(),
    eventNo_(db().getEvent()),
    isPtr_(nullptr)
{
    if (registerCopy && rio.registered_)
    {
        const_cast<regIOobject&>(rio).checkOut();
        checkIn();
    }
}


Foam::regIOobject::regIOobject
(
    const word& newName,
    const regIOobject& rio,
    bool registerCopy
)
:
    IOobject(newName, rio.instance(), rio.local(), rio.db()),
    registered_(false),
    ownedByRegistry_(false),
    watchIndices_(),
    eventNo_(db().getEvent()),
    isPtr_(nullptr)
{
    if (registerCopy)
    {
        checkIn();
    }
}


Foam::regIOobject::regIOobject
(
    const IOobject& io,
    const regIOobject& rio
)
:
    IOobject(io),
    registered_(false),
    ownedByRegistry_(false),
    watchIndices_(),
    eventNo_(db().getEvent()),
    isPtr_(nullptr)
{
    if (registerObject())
    {
        checkIn();
    }
}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::regIOobject::~regIOobject()
{
    if (objectRegistry::debug)
    {
        Pout<< "Destroying regIOobject called " << name()
            << " of type " << type()
            << " registered " << registered_
            << " owned " << ownedByRegistry_
            << " in directory " << path()
            << endl;
    }

    // Check out of objectRegistry if not owned by the registry
    if (!ownedByRegistry_)
    {
        checkOut();
    }
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::regIOobject::checkIn()
{
    if (!registered_)
    {
        // multiple checkin of same object is disallowed - this would mess up
        // any mapping
        registered_ = db().checkIn(*this);

        // check-in on defaultRegion is allowed to fail, since subsetted meshes
        // are created with the same name as their originating mesh
        if (!registered_ && debug && name() != polyMesh::defaultRegion)
        {
            if (debug == 2)
            {
                // for ease of finding where attempted duplicate check-in
                // originated
                FatalErrorInFunction
                    << "failed to register object " << objectPath()
                    << " the name already exists in the objectRegistry" << endl
                    << "Contents:" << db().sortedToc()
                    << abort(FatalError);
            }
            else
            {
                WarningInFunction
                    << "failed to register object " << objectPath()
                    << " the name already exists in the objectRegistry"
                    << endl;
            }
        }
    }

    return registered_;
}


bool Foam::regIOobject::checkOut()
{
    if (registered_)
    {
        registered_ = false;

        forAllReverse(watchIndices_, i)
        {
            fileHandler().removeWatch(watchIndices_[i]);
        }
        watchIndices_.clear();
        return db().checkOut(*this);
    }

    return false;
}


Foam::label Foam::regIOobject::addWatch(const fileName& f)
{
    label index = -1;

    if
    (
        registered_
     && readOpt() == MUST_READ_IF_MODIFIED
     && time().runTimeModifiable()
    )
    {
        index = fileHandler().findWatch(watchIndices_, f);

        if (index == -1)
        {
            index = watchIndices_.size();
            watchIndices_.append(fileHandler().addWatch(f));
        }
    }
    return index;
}


void Foam::regIOobject::addWatch()
{
    if
    (
        registered_
     && readOpt() == MUST_READ_IF_MODIFIED
     && time().runTimeModifiable()
    )
    {
        fileName f = filePath();
        if (!f.size())
        {
            // We don't have this file but would like to re-read it.
            // Possibly if master-only reading mode.
            f = objectPath();
        }

        label index = fileHandler().findWatch(watchIndices_, f);
        if (index != -1)
        {
            FatalErrorInFunction
                << "Object " << objectPath() << " of type " << type()
                << " already watched with index " << watchIndices_[index]
                << abort(FatalError);
        }

        // If master-only reading only the master will have all dependencies
        // so scatter these to slaves
        bool masterOnly =
            global()
         && (
                regIOobject::fileModificationChecking == timeStampMaster
             || regIOobject::fileModificationChecking == inotifyMaster
            );

        if (masterOnly && Pstream::parRun())
        {
            // Get master watched files
            fileNameList watchFiles;
            if (Pstream::master())
            {
                watchFiles.setSize(watchIndices_.size());
                forAll(watchIndices_, i)
                {
                    watchFiles[i] = fileHandler().getFile(watchIndices_[i]);
                }
            }
            Pstream::scatter(watchFiles);

            if (!Pstream::master())
            {
                // unregister current ones
                forAllReverse(watchIndices_, i)
                {
                    fileHandler().removeWatch(watchIndices_[i]);
                }

                watchIndices_.clear();
                forAll(watchFiles, i)
                {
                    watchIndices_.append(fileHandler().addWatch(watchFiles[i]));
                }
            }
        }

        watchIndices_.append(fileHandler().addWatch(f));
    }
}


bool Foam::regIOobject::upToDate(const regIOobject& a) const
{
    label da = a.eventNo()-eventNo_;

    // In case of overflow *this.event() might be 2G but a.event() might
    // have overflowed to 0.
    // Detect this by detecting a massive difference (labelMax/2) between
    // the two events.
    //
    //  a       *this   return
    //  -       -----   ------
    // normal operation:
    //  11      10      false
    //  11      11      false
    //  10      11      true
    // overflow situation:
    //  0       big     false
    //  big     0       true

    if (da > labelMax/2)
    {
        // *this.event overflowed but a.event not yet
        return true;
    }
    else if (da < -labelMax/2)
    {
        // a.event overflowed but *this not yet
        return false;
    }
    else if (da < 0)
    {
        // My event number higher than a
        return true;
    }
    else
    {
        return false;
    }
}


bool Foam::regIOobject::upToDate
(
    const regIOobject& a,
    const regIOobject& b
) const
{
    return upToDate(a) && upToDate(b);
}


bool Foam::regIOobject::upToDate
(
    const regIOobject& a,
    const regIOobject& b,
    const regIOobject& c
) const
{
    return upToDate(a) && upToDate(b) && upToDate(c);
}


bool Foam::regIOobject::upToDate
(
    const regIOobject& a,
    const regIOobject& b,
    const regIOobject& c,
    const regIOobject& d
) const
{
    return upToDate(a) && upToDate(b) && upToDate(c) && upToDate(d);
}


void Foam::regIOobject::setUpToDate()
{
    eventNo_ = db().getEvent();
}


void Foam::regIOobject::rename(const word& newName)
{
    // Check out of objectRegistry
    checkOut();

    IOobject::rename(newName);

    if (registerObject())
    {
        // Re-register object with objectRegistry
        checkIn();
    }
}


Foam::fileName Foam::regIOobject::filePath() const
{
    return localFilePath(type());
}


bool Foam::regIOobject::headerOk()
{
    // Note: Should be consistent with IOobject::typeHeaderOk(false)

    bool ok = true;

    fileName fName(filePath());

    ok = Foam::fileHandler().readHeader(*this, fName, type());

    if (!ok && IOobject::debug)
    {
        IOWarningInFunction(fName)
            << "failed to read header of file " << objectPath()
            << endl;
    }

    return ok;
}


void Foam::regIOobject::operator=(const IOobject& io)
{
    // Close any file
    isPtr_.clear();

    // Check out of objectRegistry
    checkOut();

    IOobject::operator=(io);

    if (registerObject())
    {
        // Re-register object with objectRegistry
        checkIn();
    }
}


// ************************************************************************* //
