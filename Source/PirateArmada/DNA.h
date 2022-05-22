// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "CoreMinimal.h"

/**
 * 
 */
class PIRATEARMADA_API DNA
{
public:
	DNA();
	DNA(int DNAsize);
	~DNA();

	int NumOfStrengthValues;
	TArray<float> StrengthValues;
	float MUTATION_CHANCE = 0.20f;
	float m_storedfitness = -1;

	DNA Crossover(DNA otherDNA);
	void Mutation();
};
