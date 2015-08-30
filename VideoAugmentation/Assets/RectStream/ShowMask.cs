using UnityEngine;
using System.Collections;

public class ShowMask : MonoBehaviour {

	public Texture2D		mMaskTexture;
	public RectStreamParser	mRectStreamParser;

	[Range(0,1)]
	public float			mDeltaMultiplier = 0.1f;
	[Range(0,20)]
	public float			mTime = 0;

	void Update () {

		if (mMaskTexture == null || mRectStreamParser == null)
			return;

		mTime += Time.deltaTime * mDeltaMultiplier;

		mRectStreamParser.LoadMaskTexture (mMaskTexture, mTime);
	
	}
}
