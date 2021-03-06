    Info<< "Reading physicalProperties\n" << endl;

    IOdictionary physicalProperties
    (
        IOobject
        (
            "physicalProperties",
            runTime.constant(),
            mesh,
            IOobject::MUST_READ_IF_MODIFIED,
            IOobject::NO_WRITE
        )
    );

    dimensionedScalar epsilon0
    (
        "epsilon0",
        dimensionSet(-1, -3, 4, 0, 0, 2, 0),
        physicalProperties
    );

    dimensionedScalar k
    (
        "k",
        dimensionSet(-1, 0, 2, 0, 0, 1, 0),
        physicalProperties
    );


    Info<< "Reading field phi\n" << endl;
    volScalarField phi
    (
        IOobject
        (
            "phi",
            runTime.timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    );


    Info<< "Reading field rho\n" << endl;
    volScalarField rho
    (
        IOobject
        (
            "rho",
            runTime.timeName(),
            mesh,
            IOobject::MUST_READ,
            IOobject::AUTO_WRITE
        ),
        mesh
    );


    Info<< "Calculating field rhoFlux\n" << endl;
    surfaceScalarField rhoFlux
    (
        IOobject
        (
            "rhoFlux",
            runTime.timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::NO_WRITE
        ),
        -k*mesh.magSf()*fvc::snGrad(phi)
    );
