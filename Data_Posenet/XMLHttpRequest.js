function ToHexString(Bytes,JoinChar,MaxBytes)
{
	if ( MaxBytes === undefined )
		MaxBytes = Bytes.byteLength;
	let Length = Math.min( MaxBytes, Bytes.byteLength );
	
	if ( JoinChar === undefined )
		JoinChar = '';
	
	let Str = '';
	let PushByte = function(byte)
	{
		Str += ('0' + (byte & 0xFF).toString(16)).slice(-2);
		Str += JoinChar;
	}
	
	for ( let i=0;	i<Length;	i++ )
		PushByte(Bytes[i]);
	
	//Bytes.forEach(PushByte);
	return Str;
}



String.prototype.replaceAll = function(search, replace)
{
	//	https://stackoverflow.com/questions/1967119/why-does-javascript-replace-only-first-instance-when-using-replace
	if (replace === undefined) {
		return this.toString();
	}
	return this.split(search).join(replace);
}

function GetLocalFilenameOfUrl(Url)
{
	var UrlRegex = new RegExp('^http[s]://([a-zA-z\-.]+)/(.*)');
	var Match = Url.match( UrlRegex );
	
	let Domain = Match[1];
	let Path = Match[2];
	
	//	gr: maybe some better local cache thing
	//if ( Domain == 'storage.googleapis.com' )
	let PathParts = Path.split('/');
	let Filename = PathParts.pop();
	let LocalPath = PathParts.join('_');
	LocalPath = "Data_Posenet/" + LocalPath + "/" + Filename;
	//Debug("Converted " + Url + " to " + Filename);
	
	return LocalPath;
}




//	wrapper for file loading in posenet
function XMLHttpRequest()
{
	//Debug("Created a XMLHttpRequest");
	this.status = 404;
	this.Filename = null;
	this.responseType = 'string';
	this.responseText = null;
	this.response = null;
	
	this.open = function(RequestMode,Url)
	{
		//Debug("XMLHttpRequest.open( " + RequestMode + " " + Url );
		
		try
		{
			this.Filename = GetLocalFilenameOfUrl( Url );
		}
		catch(e)
		{
			Debug(e);
		}
	}
	
	this.onload = function()
	{
		Debug("OnLoad");
	}
	
	this.onerror = function(Error)
	{
		Debug("OnError(" + Error +")");
	}
	
	this.send = function()
	{
		try
		{
			//Debug("Requesting " + this.Filename + " as " + this.responseType );
			
			if ( this.responseType == 'string' )
			{
				let Contents = LoadFileAsString(this.Filename);
				this.responseText = Contents;
				Debug("Loaded: " + this.Filename + " length: " + Contents.length );
				//Debug( Contents.subString(0,40) );
			}
			else if ( this.responseType == 'arraybuffer' )
			{
				let Contents = LoadFileAsArrayBuffer(this.Filename)
				this.response = Contents;
				Debug("Loaded: " + this.Filename + " byte length: " + Contents.byteLength );
				//Debug( ToHexString(Contents,' ',40) + "..." );
			}
			else
			{
				throw "Don't know how to load url/file as " + this.responseType;
			}
			
			this.status = 200;
			this.onload();
		}
		catch(e)
		{
			Debug("XMLHttpRequest error: " + e);
			this.onerror(e);
		}
	}
}
