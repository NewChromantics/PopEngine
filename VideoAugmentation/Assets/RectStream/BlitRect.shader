Shader "NewChromantics/BlitRect" {
	Properties {
		_MainTex ("_MainTex", 2D) = "white" {}
		RectTexture("RectTexture", 2D) = "black" {}
		SourceMinMax("SourceMinMax", VECTOR) = (0,0,0.1,0.1)
		DestMinMax("DestMinMax", VECTOR) = (0,0,1,1)
	}
	SubShader {
		Tags { "RenderType"="Opaque" }
		LOD 200
		
		Pass
		{
		CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag

			sampler2D _MainTex;
			sampler2D RectTexture;
			float4 RectTexture_TexelSize;
			float4 SourceMinMax;
			float4 DestMinMax;
	
			struct VertexInput {
				float4 Position : POSITION;
				float2 uv_MainTex : TEXCOORD0;
			};
			
			struct FragInput {
				float4 Position : SV_POSITION;
				float2	uv_MainTex : TEXCOORD0;
			};

			
			FragInput vert(VertexInput In) {
				FragInput Out;
				Out.Position = mul (UNITY_MATRIX_MVP, In.Position );
				Out.uv_MainTex = In.uv_MainTex;
				return Out;
			}
			
			float range(float Value,float Min,float Max)
			{
				return (Value - Min) / (Max-Min);
			}

			float GetMaskDistance(float2 RectUv,float2 RectMin,float2 RectMax)
			{
				float MaxDistance = 0;
				float NearDistance = 9999;
				float SampleScale = 2.5;
				
				//	sample coords
				const int SampleCount = 25;
				/*
				const float2 Samples[SampleCount] = 
				{
					float2(-1,-1),	float2(0,-1),	float2(1,-1),
					float2(-1,0),	float2(0,0),	float2(1,0),
					float2(-1,1),	float2(0,1),	float2(1,1),
				};
				*/
				const float2 Samples[SampleCount] = 
				{
					float2(-2,-2),	float2(-1,-2),	float2(0,-2),	float2(1,-2),	float2(2,-2),
					float2(-2,-1),	float2(-1,-1),	float2(0,-1),	float2(1,-1),	float2(2,-1),
					float2(-2,0),	float2(-1,0),	float2(0,0),	float2(1,0),	float2(2,0),
					float2(-2,1),	float2(-1,1),	float2(0,1),	float2(1,1),	float2(2,1),
					float2(-2,2),	float2(-1,2),	float2(0,2),	float2(1,2),	float2(2,2),
				};
				
				for ( int i=0;	i<SampleCount;	i++ )
				{
					float2 SampleOffset = Samples[i];
					MaxDistance = max( MaxDistance, length(SampleOffset) );
					
					float2 Uv = RectUv + SampleOffset*RectTexture_TexelSize.xy * SampleScale;
					//	auto fail if outside our local uv
					if ( Uv.x < RectMin.x || Uv.y < RectMin.y || Uv.x > RectMax.x || Uv.y > RectMax.y )
						continue;

					bool Masked = tex2D( RectTexture, Uv ).a > 0.5f;
					if ( Masked )
						NearDistance = min( NearDistance, length(SampleOffset) );
				}
				return clamp( NearDistance / MaxDistance, 0, 1 );
			}

			fixed4 frag(FragInput In) : SV_Target {
				
				float2 uv = In.uv_MainTex;
				float4 Base = tex2D( _MainTex, In.uv_MainTex );
				
				float Localu = range(uv.x,DestMinMax.z,DestMinMax.x);
				float Localv = range(uv.y,DestMinMax.y,DestMinMax.w);
				float2 LocalUv = float2( Localu, Localv );
				
				//	to allow glows outside the range of the mask rect, allow further reach
				float LocalReach = 0.2f;
				float2 LocalUvMin = float2( 0-LocalReach, 0-LocalReach );
				float2 LocalUvMax = float2( 1+LocalReach, 1+LocalReach );
				
				if ( LocalUv.x >= LocalUvMin.x && LocalUv.y >= LocalUvMin.y && LocalUv.x <= LocalUvMax.x && LocalUv.y <= LocalUvMax.y )
				{
					float Rectu = lerp(SourceMinMax.z, SourceMinMax.x,LocalUv.x);
					float Rectv = lerp(SourceMinMax.y, SourceMinMax.w,LocalUv.y);
					
					float MaskDistance = GetMaskDistance( float2(Rectu,Rectv), SourceMinMax.xy, SourceMinMax.zw  );
					
					if ( MaskDistance <= 0.0f )
					{
						//	copy pixels from mask. only applies when we have RGBA masks
						//Base.xyz = tex2D( RectTexture, float2(Rectu,Rectv) ).xyz;
					}
					else
					{
						//	fade glow
						Base.xyz = lerp( Base.xyz, float3(1,0,0), 1-MaskDistance );
					}
				}

				return Base;				
			}

		ENDCG
	}
}
		
}
