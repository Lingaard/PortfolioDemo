#include "pch.h"
#include "L_System.h"

/*	Alphabet table							Macro Table
 ___________________________________				 _________________________________________________________
| F | Move forward		            |				| C | A curve											  |
|---|-------------------------------|				|---|-----------------------------------------------------|
| R | Yaw clockwise		            |				| H | A vertical ascent that returns to horizontal		  |
|---|-------------------------------|				|---|-----------------------------------------------------|
| L | Yaw counterclockwise          |				| Q | A branching structure that generates a room		  |
|---|-------------------------------|				|---|-----------------------------------------------------|
| U | Pitch up			            |				| T | Similar to the H symbol, but splits into two curves |
|---|-------------------------------|				|---|-----------------------------------------------------|
| D | Pitch down	                |				| I | Represents a straightline							  |
|---|-------------------------------|				----------------------------------------------------------
| O | Increase the angle	        |
|---|-------------------------------|				Own added macros
| A | Decrease the angle	        |
|---|-------------------------------|				J | general travel
| B | Step increase		            |				K | branching
|---|-------------------------------|
| S | Step decrease		            |
|---|-------------------------------|
| Z | The tip of a branch	        |
|---|-------------------------------|
| 0 | Stop connecting other branches|
|---|-------------------------------|
|[ ]| Start/End branch              |
------------------------------------
*/

bool L_System::isAtomic(char c)
{
	//afterthought: ...yes this should have been a string.contains()
	return (c - 'F') * (c - 'R') * (c - 'L') * (c - 'U') * (c - 'D') * (c - 'O') * (c - 'A') * (c - 'B') * (c - 'S') * (c - 'Z') * (c - '0') * (c - '[') * (c - ']') == 0;
}

L_System::L_System()
{
}

L_System::~L_System()
{
}

void L_System::createBasicRuleSet()
{
	// Several variations of the macros
	std::string C[] =
	{
		"RFRF", "LFLF","RFRFRFRF", "LFLFLFLF", "LUFLFLD", "LDFLFLU", "RUFRFRD", "RDFRFRU",
	};
	std::string H[] =
	{
		"UFFFD", "UUFFDD", "UUUFDDD", "UUUFFFDDD", 
	};
	std::string Q[] =
	{
		"O[RFLFF][LFRFF]AFFF", "[ORA[LLFFF]F[LLFFF]F[LLFFF]F[LLFFF]F]L", "[C][C]F[CC]F[C]F", 
	};
	std::string T[] =
	{
		"UUU[LFDFRRDFLD]RFDFLLDFRD"
	};
	std::string I[] =
	{
		"F", "FF", "FFF", "SFFFFFFFB", "[FFFFFFFFF]UUUUBF[DDDDFFFFFFFF]FSDDDDFFFFFFFFF"
	};

	std::string J[] =
	{
		"IC", "CC", "H", "T", "TC"
	};
	std::string K[] =
	{
		"QQQ[JJJJK][LLLDJJJK]RRRRUJJJK", "QQ[JJJJK]RRRRDJJUJK", "QQ[JJJK]UJJDJK"
	};

	std::vector<std::string> vec;
	std::vector<Rule> ruleSet;

	vec.assign(C, C + sizeof(C) / sizeof(*C));
	ruleSet.push_back(Rule('C', vec));

	vec.assign(H, H + sizeof(H) / sizeof(*H));
	ruleSet.push_back(Rule('H', vec));

	vec.assign(Q, Q + sizeof(Q) / sizeof(*Q));
	ruleSet.push_back(Rule('Q', vec));

	vec.assign(T, T + sizeof(T) / sizeof(*T));
	ruleSet.push_back(Rule('T', vec));

	vec.assign(I, I + sizeof(I) / sizeof(*I));
	ruleSet.push_back(Rule('I', vec));

	vec.assign(J, J + sizeof(J) / sizeof(*J));
	ruleSet.push_back(Rule('J', vec));

	vec.assign(K, K + sizeof(K) / sizeof(*K));
	ruleSet.push_back(Rule('K', vec));


	m_ruleSet.insert(m_ruleSet.end(), ruleSet.begin(), ruleSet.end());
}

void L_System::createTestRuleSet()
{
	std::vector<Rule> ruleSet;
	ruleSet.push_back(Rule('X', "XY"));
	ruleSet.push_back(Rule('Y', "X"));

	m_ruleSet.insert(m_ruleSet.end(), ruleSet.begin(), ruleSet.end());

}

std::string L_System::getSentence()
{
	return m_sentence;
}

void L_System::setSentence(std::string axiom)
{
	m_sentence = axiom;
}

void L_System::runIteration(int iterations)
{
	for (size_t iGenerations = 0; iGenerations < iterations; iGenerations++)
	{
		std::string next = "";
		for (std::string::iterator iter = m_sentence.begin(), end = m_sentence.end(); iter != end; iter++)
		{
			// if atomic
			if (isAtomic(*iter))
			{
				next.push_back(*iter);
			}
			else {
				// Check which rule
				for (size_t iRule = 0; iRule < m_ruleSet.size(); iRule++)
				{
					if (*iter == m_ruleSet[iRule].a)
					{
						next.append(m_ruleSet[iRule].getB());
						break;
					}
				}
			}
		}
		m_sentence = next;
		m_generation++;
	}
}

std::string L_System::runSentence(std::string axiom, int iterations)
{
	setSentence(axiom);
	runIteration(iterations);
	return m_sentence;
}
