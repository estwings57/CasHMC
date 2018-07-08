/*********************************************************************************
*  CasHMC v1.3 - 2017.07.10
*  A Cycle-accurate Simulator for Hybrid Memory Cube
*
*  Copyright 2016, Dong-Ik Jeon
*                  Ki-Seok Chung
*                  Hanyang University
*                  estwings57 [at] gmail [dot] com
*  All rights reserved.
*********************************************************************************/

#ifndef CALLBACK_H
#define CALLBACK_H

//CallBack.h

#include <stdint.h>		//uint64_t

namespace CasHMC
{

template <typename ReturnT, typename Param1T, typename Param2T>
class CallbackBase
{
public:
	virtual ~CallbackBase() = 0;
	virtual ReturnT operator()(Param1T, Param2T) = 0;
};

template <typename Return, typename Param1T, typename Param2T>
CasHMC::CallbackBase<Return,Param1T,Param2T>::~CallbackBase() {}

template <typename ConsumerT, typename ReturnT, typename Param1T, typename Param2T>
class Callback : public CallbackBase <ReturnT,Param1T,Param2T>
{
private:
	typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T);
	ConsumerT* const object;
	const PtrMember member;

public:
	Callback(ConsumerT* const object, PtrMember member):
		object(object), member(member)
	{
	}

	Callback(const Callback<ConsumerT,ReturnT,Param1T,Param2T>& e):
		object(e.object), member(e.member)
	{
	}

	ReturnT operator()(Param1T param1, Param2T param2)
	{
		return (const_cast<ConsumerT*>(object)->*member)(param1,param2);
	}
};

typedef CallbackBase<void, uint64_t, uint64_t> TransCompCB;
}

#endif