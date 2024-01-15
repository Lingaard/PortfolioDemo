#pragma once

/*	Alphabet table						Macro Table
 ___________________________________	 _________________________________________________________
| F | Move forward		            |	| C | A curve											  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| R | Yaw clockwise		            |	| H | A vertical ascent that returns to horizontal		  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| L | Yaw counterclockwise          |	| Q | A branching structure that generates a room		  |
|---|-------------------------------|	|---|-----------------------------------------------------|
| U | Pitch up			            |	| T | Similar to the H symbol, but splits into two curves |
|---|-------------------------------|	|---|-----------------------------------------------------|
| D | Pitch down	                |	| I | Represents a straightline							  |
|---|-------------------------------|	----------------------------------------------------------
| O | Increase the angle	        |
|---|-------------------------------|
| A | Decrease the angle	        |
|---|-------------------------------|
| B | Step increase		            |
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


class L_System
{
private:
	struct Rule {
		char a;
		std::vector<std::string> b;

		Rule(char in_a, std::string in_b)
		{
			a = in_a;
			b.push_back(in_b);
		}
		Rule(char in_a, std::vector<std::string> in_b)
		{
			a = in_a;
			b = in_b;
		}

		std::string getB()
		{
			int index = rand() % b.size();
			return b[index];
		}
	};

	std::string m_sentence = "";
	std::vector<Rule> m_ruleSet;

	int m_generation = 0;

	bool isAtomic(char c);

public:
	L_System();
	~L_System();

	void createBasicRuleSet();
	void createTestRuleSet();

	void setSentence(std::string axiom);
	std::string getSentence();
	void runIteration(int iterations = 1);

	std::string runSentence(std::string axiom, int iterations);

};