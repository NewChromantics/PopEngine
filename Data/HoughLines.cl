typedef float16 THoughLine;


float2 GetHoughLineStart(THoughLine HoughLine)						{	return HoughLine.xy;	}
float2 GetHoughLineEnd(THoughLine HoughLine)						{	return HoughLine.zw;	}

float GetHoughLineAngle(THoughLine HoughLine)						{	return HoughLine[4];	}
void SetHoughLineAngle(THoughLine* HoughLine,float Value)			{	(*HoughLine)[4] = Value;	}

float GetHoughLineDistance(THoughLine HoughLine)					{	return HoughLine[5];	}
void SetHoughLineDistance(THoughLine* HoughLine,float Value)		{	(*HoughLine)[5] = Value;	}

int GetHoughLineWindowIndex(THoughLine HoughLine)					{	return HoughLine[8];	}
void SetHoughLineWindowIndex(THoughLine* HoughLine,int Value)		{	(*HoughLine)[8] = Value;	}

float GetHoughLineScore(THoughLine HoughLine)						{	return HoughLine[6];	}
void SetHoughLineScore(THoughLine* HoughLine,float Value)			{	(*HoughLine)[6] = Value;	}

bool GetHoughLineVertical(THoughLine HoughLine)						{	return HoughLine[7];	}
void SetHoughLineVertical(THoughLine* HoughLine,bool Value)			{	(*HoughLine)[7] = Value?1:0;	}

float GetHoughLineMaxPixels(THoughLine HoughLine)					{	return HoughLine[9];	}
void SetHoughLineMaxPixels(THoughLine* HoughLine,float Value)		{	(*HoughLine)[9] = Value;	}

int GetHoughLineStartJointVertLineIndex(THoughLine HoughLine)					{	return HoughLine[10];	}
void SetHoughLineStartJointVertLineIndex(THoughLine* HoughLine,int Value)		{	(*HoughLine)[10] = Value;	}

int GetHoughLineEndJointVertLineIndex(THoughLine HoughLine)					{	return HoughLine[11];	}
void SetHoughLineEndJointVertLineIndex(THoughLine* HoughLine,int Value)		{	(*HoughLine)[11] = Value;	}

