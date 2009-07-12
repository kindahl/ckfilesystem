#include <iostream>
#include <ckcore/path.hh>
#include <ckcore/file.hh>
#include <ckcore/string.hh>
#include <ckcore/filestream.hh>
#include <ckcore/exception.hh>
#include "ckfilesystem/sectorstream.hh"
#include "ckfilesystem/iso9660verifier.hh"

#define CKFSVFY_VERSION			"0.1"

int main(int argc,const char *argv[])
{
	std::cout << "ckFsVerifier " << CKFSVFY_VERSION
			  << " Copyright (C) Christian Kindahl 2009" << std::endl << std::endl;

	// Parse the command line.
	if (argc != 2)
	{
		std::cerr << "Error: Invalid usage, please specify a disc image file to analyze."
				  << std::endl;
		return 1;
	}

	ckcore::Path file_path(ckcore::string::ansi_to_auto<1024>(argv[1]).c_str());
	if (!ckcore::File::exist(file_path))
	{
		std::cerr << "Error: The specified file doesn't exist." << std::endl;
		return 1;
	}

	// Prepare file stream.
	ckcore::FileInStream file_stream(file_path);
	if (!file_stream.open())
	{
		std::cerr << "Error: Unable to open file for reading." << std::endl;
		return 1;
	}

	// Verify the file system.
	try
	{
		std::wcout << ckT("Input: ") << std::endl << ckT("  File: ")
				   << file_path.name() << std::endl << ckT("  Size: ")
				   << file_stream.size() << ckT(" bytes") << std::endl
				   << std::endl;

		ckfilesystem::Iso9660Verifier verifier;

		ckfilesystem::SectorInStream in_stream(file_stream);
		verifier.verify(in_stream);
	}
	catch (ckfilesystem::VerificationException &e)
	{
		std::wcerr << ckT("=> Error: ") << e.what() << std::endl;
		std::wcerr << ckT("=>        See ") << e.reference() << std::endl;
		return 1;
	}
	catch (ckcore::Exception &e)
	{
		std::wcerr << ckT("=> Error: ") << e.what() << std::endl;
		return 1;
	}

	return 0;
}
