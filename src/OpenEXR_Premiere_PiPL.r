

type 'IMPT'
{
	longint;
};

resource 'IMPT' (1000)
{
	// drawtype - this unique fourcc is required by After Effects,
	// and is not related to the filetype(s) supported by this importer
	0x70455852	// 'pEXR'
};