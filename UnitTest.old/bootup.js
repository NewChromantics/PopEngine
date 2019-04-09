//	replace this with a proper mini unit test/api example set of scripts

Pop.Debug("Hello World");

function OnRender(RenderTarget)
{
	RenderTarget.ClearColour(255,0,0);
}

let Window = new Pop.Opengl.Window("A Pop Engine window!");
Window.OnRender = OnRender;
