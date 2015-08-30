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

			fixed4 frag(FragInput In) : SV_Target {
				
				float2 uv = In.uv_MainTex;
				float4 Base = tex2D( _MainTex, In.uv_MainTex );
				
				//	gr: for some reason this is backwards
				//float Localu = range(uv.x,DestMinMax.x,DestMinMax.z);
				float Localu = range(uv.x,DestMinMax.z,DestMinMax.x);
				float Localv = range(uv.y,DestMinMax.y,DestMinMax.w);
				float2 LocalUv = float2( Localu, Localv );
				
				if ( LocalUv.x >= 0 && LocalUv.y >= 0 && LocalUv.x <= 1 && LocalUv.y <= 1 )
				{
					//	gr: also mirrored...
					//float Rectu = lerp(SourceMinMax.x, SourceMinMax.z,LocalUv.x);
					float Rectu = lerp(SourceMinMax.z, SourceMinMax.x,LocalUv.x);
					float Rectv = lerp(SourceMinMax.y, SourceMinMax.w,LocalUv.y);
					
					float4 Mask = tex2D( RectTexture, float2(Rectu,Rectv) );
					if ( Mask.w > 0 )
					{
						Base.xyz = Mask.xyz;
					}
				}

				return Base;				
			}

		ENDCG
	}
}
		
}
