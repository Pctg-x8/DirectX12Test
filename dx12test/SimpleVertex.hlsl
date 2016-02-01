struct VSOutput
{
	float4 col : COLOR0;
	float4 pos : SV_Position;
};

VSOutput main( float4 pos : POSITION, float4 col : COLOR0 )
{
	VSOutput o;

	o.pos = pos;
	o.col = col;
	return o;
}