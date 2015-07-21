#if UNITY_EDITOR
using UnityEngine;
using System.Collections;
using UnityEditor;
using System.IO;

public class SaveTextureToPng : MonoBehaviour {

	[MenuItem("Assets/Texture/Save Texture to Png")]
	static void _SaveTextureToPng()
	{
		//	get selected textures
		string[] AssetGuids = Selection.assetGUIDs;
		for (int i=0; i<AssetGuids.Length; i++) {
			string Guid = AssetGuids[i];
			string Path = AssetDatabase.GUIDToAssetPath (Guid);
			Texture Tex = AssetDatabase.LoadAssetAtPath( Path, typeof(Texture) ) as Texture;
			if ( !Tex )
				continue;

			DoSaveTextureToPng( Tex, Path, Guid, true );
		}
	}

	static public bool DoSaveTextureToPng(Texture Tex,string Path,string DefaultFilename,bool PromptFilename)
	{
		//	copy to render texture
		RenderTexture rt = RenderTexture.GetTemporary( Tex.width, Tex.height, 0, RenderTextureFormat.ARGB32 );
		rt.filterMode = FilterMode.Point;
		Graphics.Blit( Tex, rt );
		Texture2D Temp = new Texture2D( rt.width, rt.height, TextureFormat.RGB24, false );
		RenderTexture.active = rt;
		Temp.ReadPixels( new Rect(0,0,rt.width,rt.height), 0, 0 );
		Temp.Apply();
		RenderTexture.active = null;
		RenderTexture.ReleaseTemporary( rt );

		byte[] Bytes = Temp.EncodeToPNG();

		string Filename = DefaultFilename;
		if ( PromptFilename )
		{
			Filename = EditorUtility.SaveFilePanel("save " + Path, "", DefaultFilename, "png");
			if ( Filename.Length == 0 )
				return false;
		}

		File.WriteAllBytes( Filename, Bytes );
		return true;
	}
}

#endif
