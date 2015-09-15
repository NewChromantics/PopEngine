﻿using UnityEngine;
using System.Collections;

public class ShowMask : MonoBehaviour {

	public RenderTexture	mMovieTexture;
	public string			mMovieFilename;
	public PopMovie			mMovie;
	public Texture2D		mMaskTexture;
	public RectStreamParser	mRectStreamParser;
	public Shader			mBlitRectShader;

	[Range(0,1)]
	public float			mDeltaMultiplier = 0.1f;
	[Range(0,20)]
	public float			mTime = 0;
	[Range(-1,1)]
	public float			mTimeOffset = 0;

	void Start()
	{
		PopMovieParams Params = new PopMovieParams ();
		mMovie = new PopMovie (mMovieFilename, Params,false);

		mMovie.AddDebugCallback (Debug.Log);
	}

	void Update () {

		mTime += Time.deltaTime * mDeltaMultiplier;

		//	update textures
		var Frame = mRectStreamParser.GetFrame (mTime+mTimeOffset);

		if (mMaskTexture != null && mRectStreamParser != null && Frame != null ) {
			mRectStreamParser.LoadMaskTexture (mMaskTexture, Frame);
		}

		if ( mMovie !=null && mMovieTexture != null )
		{
			mMovie.SetTime( mTime );
			mMovie.Update();
			mMovie.UpdateTexture( mMovieTexture );
		}

		if (mBlitRectShader) {

			Material BlitMat = new Material( mBlitRectShader );

			//	flip between textures
			RenderTexture TempIn = RenderTexture.GetTemporary( mMovieTexture.width, mMovieTexture.height );
			RenderTexture TempOut = RenderTexture.GetTemporary( mMovieTexture.width, mMovieTexture.height );

			//	init to TempOut as its swapped immediately in the loop
			Graphics.Blit( mMovieTexture, TempOut );

			//	merge
			if (Frame != null) {
				foreach (Tuple<Rect,Rect> Rects in Frame.mRects) {
					RenderTexture Swap = TempIn;
					TempIn = TempOut;
					TempOut = Swap;
					Rect SourceRect = Rects.Second;
					Rect DestRect = Rects.First;
					Vector4 SourceMinMax = new Vector4( SourceRect.xMin, SourceRect.yMin, SourceRect.xMax, SourceRect.yMax );
					Vector4 DestMinMax = new Vector4( DestRect.xMin, 1-DestRect.yMin, DestRect.xMax, 1-DestRect.yMax );
					BlitMat.SetTexture("RectTexture", mMaskTexture );
					BlitMat.SetVector("DestMinMax", DestMinMax );
					BlitMat.SetVector("SourceMinMax", SourceMinMax );
					Graphics.Blit( TempIn, TempOut, BlitMat );
				}
			}

			Graphics.Blit( TempOut, mMovieTexture );
			RenderTexture.ReleaseTemporary( TempIn );
			RenderTexture.ReleaseTemporary( TempOut );

		}
	}
}
