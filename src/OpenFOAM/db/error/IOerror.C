/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
     \\/     M anipulation  | Copyright (C) 2015 OpenCFD Ltd.
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

#include "error.H"
#include "StringStream.H"
#include "fileName.H"
#include "dictionary.H"
#include "JobInfo.H"
#include "Pstream.H"

// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::IOerror::IOerror(const string& title)
:
    error(title),
    ioFileName_("unknown"),
    ioStartLineNumber_(-1),
    ioEndLineNumber_(-1)
{}


Foam::IOerror::IOerror(const dictionary& errDict)
:
    error(errDict),
    ioFileName_(errDict.get<string>("ioFileName")),
    ioStartLineNumber_(errDict.get<label>("ioStartLineNumber")),
    ioEndLineNumber_(errDict.get<label>("ioEndLineNumber"))
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::IOerror::~IOerror() throw()
{}


// * * * * * * * * * * * * * * * Member Operators  * * * * * * * * * * * * * //

Foam::OSstream& Foam::IOerror::operator()
(
    const char* functionName,
    const char* sourceFileName,
    const int sourceFileLineNumber,
    const string& ioFileName,
    const label ioStartLineNumber,
    const label ioEndLineNumber
)
{
    error::operator()(functionName, sourceFileName, sourceFileLineNumber);
    ioFileName_ = ioFileName;
    ioStartLineNumber_ = ioStartLineNumber;
    ioEndLineNumber_ = ioEndLineNumber;

    return operator OSstream&();
}


Foam::OSstream& Foam::IOerror::operator()
(
    const char* functionName,
    const char* sourceFileName,
    const int sourceFileLineNumber,
    const IOstream& ioStream
)
{
    return operator()
    (
        functionName,
        sourceFileName,
        sourceFileLineNumber,
        ioStream.name(),
        ioStream.lineNumber(),
        -1
    );
}


Foam::OSstream& Foam::IOerror::operator()
(
    const char* functionName,
    const char* sourceFileName,
    const int sourceFileLineNumber,
    const dictionary& dict
)
{
    return operator()
    (
        functionName,
        sourceFileName,
        sourceFileLineNumber,
        dict.name(),
        dict.startLineNumber(),
        dict.endLineNumber()
    );
}


void Foam::IOerror::SafeFatalIOError
(
    const char* functionName,
    const char* sourceFileName,
    const int sourceFileLineNumber,
    const IOstream& ioStream,
    const string& msg
)
{
    if (JobInfo::constructed)
    {
        FatalIOError
        (
            functionName,
            sourceFileName,
            sourceFileLineNumber,
            ioStream
        )   << msg << Foam::exit(FatalIOError);
    }
    else
    {
        std::cerr
            << nl
            << "--> FOAM FATAL IO ERROR:" << nl
            << msg
            << nl
            << "file: " << ioStream.name()
            << " at line " << ioStream.lineNumber() << '.'
            << nl << nl
            << "    From function " << functionName
            << nl
            << "    in file " << sourceFileName
            << " at line " << sourceFileLineNumber << '.'
            << std::endl;
        ::exit(1);
    }
}


Foam::IOerror::operator Foam::dictionary() const
{
    dictionary errDict(error::operator dictionary());

    errDict.remove("type");
    errDict.add("type", word("Foam::IOerror"));

    errDict.add("ioFileName", ioFileName());
    errDict.add("ioStartLineNumber", ioStartLineNumber());
    errDict.add("ioEndLineNumber", ioEndLineNumber());

    return errDict;
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

void Foam::IOerror::exit(const int)
{
    if (!throwExceptions_ && JobInfo::constructed)
    {
        jobInfo.add("FatalIOError", operator dictionary());
        jobInfo.exit();
    }

    if (env("FOAM_ABORT"))
    {
        abort();
    }

    if (Pstream::parRun())
    {
        Perr<< endl << *this << endl
            << "\nFOAM parallel run exiting\n" << endl;
        Pstream::exit(1);
    }
    else
    {
        if (throwExceptions_)
        {
            // Make a copy of the error to throw
            IOerror errorException(*this);

            // Reset the message buffer for the next error message
            messageStreamPtr_->reset();

            throw errorException;
        }
        else
        {
            Perr<< endl << *this << endl
                << "\nFOAM exiting\n" << endl;
            ::exit(1);
        }
    }
}


void Foam::IOerror::abort()
{
    if (!throwExceptions_ && JobInfo::constructed)
    {
        jobInfo.add("FatalIOError", operator dictionary());
        jobInfo.abort();
    }

    if (env("FOAM_ABORT"))
    {
        Perr<< endl << *this << endl
            << "\nFOAM aborting (FOAM_ABORT set)\n" << endl;
        printStack(Perr);
        ::abort();
    }

    if (Pstream::parRun())
    {
        Perr<< endl << *this << endl
            << "\nFOAM parallel run aborting\n" << endl;
        printStack(Perr);
        Pstream::abort();
    }
    else
    {
        if (throwExceptions_)
        {
            // Make a copy of the error to throw
            IOerror errorException(*this);

            // Reset the message buffer for the next error message
            messageStreamPtr_->reset();

            throw errorException;
        }
        else
        {
            Perr<< endl << *this << endl
                << "\nFOAM aborting\n" << endl;
            printStack(Perr);
            ::abort();
        }
    }
}


void Foam::IOerror::write(Ostream& os, const bool includeTitle) const
{
    if (!os.bad())
    {
        os  << nl;
        if (includeTitle)
        {
            os  << title().c_str() << nl;
        }
        os  << message().c_str() << nl << endl;

        os  << "file: " << ioFileName().c_str();

        if (ioStartLineNumber() >= 0 && ioEndLineNumber() >= 0)
        {
            os  << " from line " << ioStartLineNumber()
                << " to line " << ioEndLineNumber() << '.';
        }
        else if (ioStartLineNumber() >= 0)
        {
            os  << " at line " << ioStartLineNumber() << '.';
        }

        if (IOerror::level >= 2 && sourceFileLineNumber())
        {
            os  << nl << nl
                << "    From function " << functionName().c_str() << endl
                << "    in file " << sourceFileName().c_str()
                << " at line " << sourceFileLineNumber() << '.';
        }
    }
}


Foam::Ostream& Foam::operator<<(Ostream& os, const IOerror& err)
{
    err.write(os);

    return os;
}


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
// Global error definitions

Foam::IOerror Foam::FatalIOError("--> FOAM FATAL IO ERROR: ");


// ************************************************************************* //
