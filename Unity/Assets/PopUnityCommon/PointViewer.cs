using UnityEngine;
using System.Collections;
using System.Collections.Generic;


public class GuiColourTexture
{
	static private Dictionary<Color,Texture2D>	mColourTextures = new Dictionary<Color,Texture2D>();
	static public Texture2D	GetTexture(Color Colour)
	{
		if (!mColourTextures.ContainsKey (Colour)) {
			Texture2D texture = new Texture2D (1, 1);
			texture.wrapMode = TextureWrapMode.Repeat;
			texture.SetPixel (0, 0, Color.white);
			texture.Apply ();
			mColourTextures [Colour] = texture;
		}
		return mColourTextures [Colour];
	}			
};


[ExecuteInEditMode]
public class PointViewer : MonoBehaviour {

	public Color		mMarkerColour = Color.white;
	public Vector2[]	mPoints = new Vector2[4]
	{
		new Vector2(0,0), 
		new Vector2(1,0),
		new Vector2(1,1),
		new Vector2(0,1)
	};
	public Rect			mEditorRect = new Rect( 0,0, 0.5f, 0.5f );
	public Texture		mTexture;
	[Range(0.001f,0.10f)]
	public float		mPointSize = 0.01f;


	public Rect GetRect()
	{
		if (mPoints == null || mPoints.Length == 0)
			return new Rect(0,0,1,1);
		Vector2 min = mPoints [0];
		Vector2 max = mPoints [0];
		for (int i=1; i<mPoints.Length; i++) {
			min.x = Mathf.Min (min.x, mPoints [i].x);
			min.y = Mathf.Min (min.y, mPoints [i].y);
			max.x = Mathf.Max (max.x, mPoints [i].x);
			max.y = Mathf.Max (max.y, mPoints [i].y);
		}
		return new Rect (min.x, min.y, max.x - min.x, max.y - min.y);
	}

	// Update is called once per frame
	public void Update () {
		
	}
	
	
	void DrawPoint(Vector2 Uv,Color Colour,string Label)
	{
		//	move into screen space
		Rect PosRect = new Rect (Uv.x, Uv.y, mPointSize, mPointSize);
		PosRect = PopMath.RectMult (PosRect, mEditorRect);
		PosRect = PopMath.RectToScreen (PosRect);
		PosRect.height = PosRect.width;
		GUI.skin.box.normal.background = GuiColourTexture.GetTexture(Colour);

		GUI.Box( PosRect, new GUIContent() );

		if (Label != null) {
			Rect TextRect = new Rect (PosRect);
			TextRect.x -= TextRect.width;
			TextRect.y -= TextRect.height;
			GUIStyle TextStyle = new GUIStyle ();
			TextStyle.fontSize = (int)(TextRect.height *3);
			TextStyle.normal.textColor = Color.black;
			GUI.Label (TextRect, Label, TextStyle);	
		}


	}
	
	
	public void OnGUI()
	{
		Rect DisplayRect = PopMath.RectToScreen (mEditorRect);
		if ( mTexture )
			GUI.DrawTexture (DisplayRect, mTexture);
		
		for (int i=0; i<mPoints.Length; i++) {
			DrawPoint( mPoints[i], mMarkerColour, "x"+i );
		}
	}
}
