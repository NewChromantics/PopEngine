using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.IO;

public class Player
{
	public Rect		mUv;	//	detected rect in camera texture

	public Player(Rect uv)
	{
		mUv = uv;
	}

	//	this may change later, so put in a func
	public Vector2	GetFootUv()	
	{
		return new Vector2( Mathf.Lerp(mUv.xMin,mUv.xMax,0.5f), mUv.yMax );
	}


	public Vector2	GetUvTopLeft()	
	{
		return mUv.min;
	}
	
	public Vector2	GetUvBottomRight()	
	{
		return mUv.max;
	}

};


public class PlayerTracker : MonoBehaviour {

	public List<Player>	mPlayers;
	public Texture		mCameraTexture;
	[Range(4,60)]
	public float		mHeadShotRectSize = 10;

	//	gr: shouldn't need these
	public int 			mCylinderPixelWidth = 10;
	public int 			mCylinderPixelHeight = 19;
	[Range(1,1000)]
	public int			mLimitRenderCount = 100;
	public int 			Gui_Depth = 0;

	public void OnPlayersChanged()
	{
	}

	void OnGUI()
	{	
		GUI.depth = Gui_Depth;
		Texture mHeadShotTexture = mCameraTexture;

		if (mHeadShotTexture && mPlayers!=null && mPlayers.Count > 0) {
			
			Rect HeadShotRect = new Rect(0,0,mHeadShotRectSize,mHeadShotRectSize);
			float CylinderWidth = mCylinderPixelWidth;
			float CylinderHeight = mCylinderPixelHeight;
				
			//	correct headshot for detection aspect ratio for a nicer output
			HeadShotRect.height = HeadShotRect.width * (CylinderHeight / CylinderWidth);
			HeadShotRect.width /= Screen.width;
			HeadShotRect.height /= Screen.height;

			for ( int i=0;	i<Mathf.Min(mLimitRenderCount,mPlayers.Count);	i++ )
			{
				var Player = mPlayers[i];

				//	get UV sample rect
				Rect UvHeadShotRect = Player.mUv;

				GUI.DrawTextureWithTexCoords (PopMath.RectToScreen (HeadShotRect), mHeadShotTexture, PopMath.ViewRectToTextureRect (UvHeadShotRect, mHeadShotTexture));

				HeadShotRect.x += HeadShotRect.width;
				if (HeadShotRect.xMax > 1.0f) {
					HeadShotRect.x = 0;
					HeadShotRect.y += HeadShotRect.height;
				}
			}
		}
	}
}
