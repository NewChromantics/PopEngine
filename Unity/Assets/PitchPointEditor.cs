using UnityEngine;
using System.Collections;
using System.Collections.Generic;



public delegate string TUniformGetUrlDelegate<T>(TUniform<T> Uniform,T Value);

public abstract class BaseUniform
{
	public string	mName;

	public abstract string	GetUrlParam ();
};

public class TUniform<T> : BaseUniform
{
	public T		mValue;
	public TUniformGetUrlDelegate<T>	mGetUrlFunc;

	public TUniform(string Name,T Value,TUniformGetUrlDelegate<T> GetUrlParamFunc=null)
	{
		mName = Name;
		mValue = Value;
		mGetUrlFunc = GetUrlParamFunc;
	}

	public override string	GetUrlParam()
	{
		if (mGetUrlFunc!=null)
			return mGetUrlFunc (this,mValue);

		return mName + "=" + mValue;
	}
};

class PopUnity
{
	public static string GetUrlParam(TUniform<Vector2> Uniform,Vector2 mValue)	
	{
		return Uniform.mName + "=" + mValue.x + "x" + mValue.y;
	}
};


[ExecuteInEditMode]
public class PitchPointEditor : PointEditor {

	public string	mServerAddress = "http://localhost:8080/setuniform/?";
	public string	mFilterName = "input01";

	override public void OnPointsChanged()
	{
		BaseUniform[] Uniforms = new BaseUniform[]
		{
			new TUniform<Vector2>("MaskTopLeft",mPoints[0],PopUnity.GetUrlParam),
			new TUniform<Vector2>("MaskTopRight",mPoints[1],PopUnity.GetUrlParam),
			new TUniform<Vector2>("MaskBottomRight",mPoints[2],PopUnity.GetUrlParam),
			new TUniform<Vector2>("MaskBottomLeft",mPoints[3],PopUnity.GetUrlParam),
		};
		StartCoroutine ("SendUniform", Uniforms );
	}

	
	IEnumerator SendUniform(BaseUniform[] Uniforms)
	{
		string Url = mServerAddress;
		Url += "filter" + "=" + mFilterName + "&";

		foreach (var Uniform in Uniforms) {
			Url += Uniform.GetUrlParam () + "&";
		}

		WWW www = new WWW (Url);
		yield return www;
		
		if (www.error != null)
			Debug.LogWarning ("Set uniform: " + www.error);
		
		if (www.text != null && www.text.Length > 0 )
			Debug.Log ("Set uniform: " + www.text);
		
		www = null;
		yield break;
	}

	IEnumerator SendUniform(BaseUniform Uniform)
	{
		BaseUniform[] Uniforms = new BaseUniform[]
		{
			Uniform
		};
		return SendUniform (Uniforms);
	}

}
