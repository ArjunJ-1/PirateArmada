// Fill out your copyright notice in the Description page of Project Settings.


#include "DNA.h"

DNA::DNA()
{
}

DNA::DNA(int DNASize)
{
	/* A Single boid has the following values: .... */
	NumOfStrengthValues = DNASize;
	/*if(NumOfStrengthValues == 7)
	{
		StrengthValues.Add(374);
		StrengthValues.Add(963);
		StrengthValues.Add(513);
		StrengthValues.Add(564);
		StrengthValues.Add(842);
		StrengthValues.Add(615);
		StrengthValues.Add(271);
	}
	else
	{
		StrengthValues.Add(374);
		StrengthValues.Add(963);
		StrengthValues.Add(513);
		StrengthValues.Add(564);
		StrengthValues.Add(842);
		StrengthValues.Add(615);
	}*/

	for (int i = 0; i < NumOfStrengthValues; i++)
		StrengthValues.Add(FMath::RandRange(0.0, 1000.f));
}

DNA::~DNA()
{
}

DNA DNA::Crossover(DNA otherDNA)
{
	DNA retValue = DNA(NumOfStrengthValues);
	const int minVal = 2;
	const int midIndex = FMath::RandRange(minVal, NumOfStrengthValues - minVal);

	for (int i = 0; i < NumOfStrengthValues; i++)
	{
		if (i < midIndex)
			retValue.StrengthValues[i] = StrengthValues[i];
		else
			retValue.StrengthValues[i] = otherDNA.StrengthValues[i];
	}
	return retValue;
}

void DNA::Mutation()
{
	// array from 0 - 100,000
	for (int i = 0; i < NumOfStrengthValues; i++)
	{
		if (FMath::RandRange(0.0f, 1.0f) < MUTATION_CHANCE)
		{
			// Improve this code
			StrengthValues[i] = FMath::RandRange(0, 100000);
		}
	}
}
