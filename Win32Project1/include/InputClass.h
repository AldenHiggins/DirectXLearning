#ifndef __INPUTCLASS_H_
#define __INPUTCLASS_H_

class InputClass
{
public:
	InputClass();
	InputClass(const InputClass&);
	~InputClass();

	void initialize();

	void keyDown(unsigned int);
	void keyUp(unsigned int);

	bool isKeyDown(unsigned int);
private:
	bool m_keys[256];
};

#endif //__INPUTCLASS_H_
