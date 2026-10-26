#include <vector>
#include <fstream>
#include <algorithm>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <exception>

#include "libwap.h"
#include "WapStream.h"
#include "Miniz.h"
#include "Util.h"

const uint32_t EXPECTED_HEADER_SIZE = 1524;

static void ReadRect(InputStream &stream, WwdRect &rect) 
{
    stream.read(rect.left);
    stream.read(rect.top);
    stream.read(rect.right);
    stream.read(rect.bottom);
}

static void ReadPlaneTiles(WwdPlane* plane, InputStream& inputStream)
{
    uint32_t i;

    if (plane->tilesCount > 0)
    {
        // Allocate space for plane's tiles
        plane->tiles = new int32_t[plane->tilesCount];

        // Move cursor to plane's tiles beginning
        inputStream.seek(plane->properties.tilesOffset);

        // Read plane's tiles
        for (i = 0; i < plane->tilesCount; i++)
        {
            inputStream.read(plane->tiles[i]);
        }
    }
}

static void ReadPlaneImageSets(WwdPlane* plane, InputStream& inputStream)
{
    uint32_t i;

    if (plane->imageSetsCount > 0)
    {
        // Allocate space for plane's image sets
        plane->imageSets = new char*[plane->imageSetsCount];

        // Move cursor to plane's image sets beginning
        inputStream.seek(plane->properties.imageSetsOffset);

        for (i = 0; i < plane->imageSetsCount; i++)
        {
            // Allocate and set read null terminated string
            plane->imageSets[i] = ReadNullTerminatedString(inputStream);
        }
    }
}

static void ReadPlaneObjects(WwdPlane* plane, InputStream& inputStream)
{
    uint32_t i;

    if (plane->objectsCount > 0)
    {
        // Allocate space for plane's objects
        plane->objects = new WwdObject[plane->objectsCount];

        // move cursor to plane's objects beginning
        inputStream.seek(plane->properties.objectsOffset);

        for (i = 0; i < plane->objectsCount; i++)
        {
            inputStream.read(plane->objects[i].id);
            inputStream.read(plane->objects[i].nameLength);
            inputStream.read(plane->objects[i].logicLength);
            inputStream.read(plane->objects[i].imageSetLength);
            inputStream.read(plane->objects[i].soundLength);
            inputStream.read(plane->objects[i].x);
            inputStream.read(plane->objects[i].y);
            inputStream.read(plane->objects[i].z);
            inputStream.read(plane->objects[i].i);
            inputStream.read(plane->objects[i].addFlags);
            inputStream.read(plane->objects[i].dynamicFlags);
            inputStream.read(plane->objects[i].drawFlags);
            inputStream.read(plane->objects[i].userFlags);
            inputStream.read(plane->objects[i].score);
            inputStream.read(plane->objects[i].points);
            inputStream.read(plane->objects[i].powerup);
            inputStream.read(plane->objects[i].damage);
            inputStream.read(plane->objects[i].smarts);
            inputStream.read(plane->objects[i].health);

            ReadRect(inputStream, plane->objects[i].moveRect);
            ReadRect(inputStream, plane->objects[i].hitRect);
            ReadRect(inputStream, plane->objects[i].attackRect);
            ReadRect(inputStream, plane->objects[i].clipRect);
            ReadRect(inputStream, plane->objects[i].userRect1);
            ReadRect(inputStream, plane->objects[i].userRect2);

            inputStream.read(plane->objects[i].userValue1);
            inputStream.read(plane->objects[i].userValue2);
            inputStream.read(plane->objects[i].userValue3);
            inputStream.read(plane->objects[i].userValue4);
            inputStream.read(plane->objects[i].userValue5);
            inputStream.read(plane->objects[i].userValue6);
            inputStream.read(plane->objects[i].userValue7);
            inputStream.read(plane->objects[i].userValue8);
            inputStream.read(plane->objects[i].minX);
            inputStream.read(plane->objects[i].minY);
            inputStream.read(plane->objects[i].maxX);
            inputStream.read(plane->objects[i].maxY);
            inputStream.read(plane->objects[i].speedX);
            inputStream.read(plane->objects[i].speedY);
            inputStream.read(plane->objects[i].tweakX);
            inputStream.read(plane->objects[i].tweakY);
            inputStream.read(plane->objects[i].counter);
            inputStream.read(plane->objects[i].speed);
            inputStream.read(plane->objects[i].width);
            inputStream.read(plane->objects[i].height);
            inputStream.read(plane->objects[i].direction);
            inputStream.read(plane->objects[i].faceDir);
            inputStream.read(plane->objects[i].timeDelay);
            inputStream.read(plane->objects[i].frameDelay);
            inputStream.read(plane->objects[i].objectType);
            inputStream.read(plane->objects[i].hitTypeFlags);
            inputStream.read(plane->objects[i].moveResX);
            inputStream.read(plane->objects[i].moveResY);

            plane->objects[i].name = ReadAndAllocateString(inputStream, plane->objects[i].nameLength);
            plane->objects[i].logic = ReadAndAllocateString(inputStream, plane->objects[i].logicLength);
            plane->objects[i].imageSet = ReadAndAllocateString(inputStream, plane->objects[i].imageSetLength);
            plane->objects[i].sound = ReadAndAllocateString(inputStream, plane->objects[i].soundLength);
            //cout << "Logic = " << plane->objects[i].logic << endl;
        }
    }
}

static void ReadPlanes(WapWwd* wapWwd, InputStream& inputStream)
{
    uint32_t i;

    // Allocate space for WWD planes
    wapWwd->planes = new WwdPlane[wapWwd->planesCount];

    // Move cursor to WWD planes beginning
    inputStream.seek(wapWwd->properties.planesOffset);

    // Read WWD planes
    for (i = 0; i < wapWwd->planesCount; i++)
    {
        // Set default values
        wapWwd->planes[i] = WwdPlane();

        /********************** PLANE'S PROPERTIES **********************/

        // Read plane's properties
        inputStream.read(wapWwd->planes[i].properties.signature);
        inputStream.read(wapWwd->planes[i].properties.null0);
        inputStream.read(wapWwd->planes[i].properties.flags);
        inputStream.read(wapWwd->planes[i].properties.null1);
        inputStream.read(wapWwd->planes[i].properties.name);
        inputStream.read(wapWwd->planes[i].properties.pixelWidth);
        inputStream.read(wapWwd->planes[i].properties.pixelHeight);
        inputStream.read(wapWwd->planes[i].properties.tilePixelWidth);
        inputStream.read(wapWwd->planes[i].properties.tilePixelHeight);
        inputStream.read(wapWwd->planes[i].properties.tilesOnAxisX);
        inputStream.read(wapWwd->planes[i].properties.tilesOnAxisY);
        inputStream.read(wapWwd->planes[i].properties.null2);
        inputStream.read(wapWwd->planes[i].properties.null3);
        inputStream.read(wapWwd->planes[i].properties.movementPercentX);
        inputStream.read(wapWwd->planes[i].properties.movementPercentY);
        inputStream.read(wapWwd->planes[i].properties.fillColor);
        inputStream.read(wapWwd->planes[i].properties.imageSetsCount);
        inputStream.read(wapWwd->planes[i].properties.objectsCount);
        inputStream.read(wapWwd->planes[i].properties.tilesOffset);
        inputStream.read(wapWwd->planes[i].properties.imageSetsOffset);
        inputStream.read(wapWwd->planes[i].properties.objectsOffset);
        inputStream.read(wapWwd->planes[i].properties.coordZ);
        inputStream.read(wapWwd->planes[i].properties.null4);
        inputStream.read(wapWwd->planes[i].properties.null5);
        inputStream.read(wapWwd->planes[i].properties.null6);

        // Data duplication for sanity reasons
        wapWwd->planes[i].tilesCount = wapWwd->planes[i].properties.tilesOnAxisX * wapWwd->planes[i].properties.tilesOnAxisY;
        wapWwd->planes[i].imageSetsCount = wapWwd->planes[i].properties.imageSetsCount;
        wapWwd->planes[i].objectsCount = wapWwd->planes[i].properties.objectsCount;
    }

    // Read plane tiles, image sets and objects
    for (i = 0; i < wapWwd->planesCount; i++)
    {
        /********************** PLANE'S TILES **********************/

        ReadPlaneTiles(&(wapWwd->planes[i]), inputStream);

        /********************** PLANE'S IMAGE SETS **********************/

        ReadPlaneImageSets(&(wapWwd->planes[i]), inputStream);

        /********************** PLANE'S OBJECTS **********************/

        ReadPlaneObjects(&(wapWwd->planes[i]), inputStream);
    }
}

void ReadTileDescriptions(WapWwd* wapWwd, InputStream& inputStream)
{
    uint32_t i;

    // Move cursor to WWD tile descriptions beginning
    inputStream.seek(wapWwd->properties.tileDescriptionsOffset);

    // Read count of tile descriptions along with junk values
    inputStream.read(32);
    inputStream.read(0);
    inputStream.read(wapWwd->tileDescriptionsCount);
    inputStream.read(0);
    inputStream.read(0);
    inputStream.read(0);
    inputStream.read(0);
    inputStream.read(0);

    // Allocate space for WWD tile descriptions
    wapWwd->tileDescriptions = new WwdTileDescription[wapWwd->tileDescriptionsCount];

    for (i = 0; i < wapWwd->tileDescriptionsCount; i++)
    {
        inputStream.read(wapWwd->tileDescriptions[i].type);
        inputStream.read(wapWwd->tileDescriptions[i].unk0);
        inputStream.read(wapWwd->tileDescriptions[i].width);
        inputStream.read(wapWwd->tileDescriptions[i].height);

        if (wapWwd->tileDescriptions[i].type == WAP_TILE_TYPE_SINGLE)
        {
            inputStream.read(wapWwd->tileDescriptions[i].insideAttrib);
        }
        else
        {
            inputStream.read(wapWwd->tileDescriptions[i].outsideAttrib);
            inputStream.read(wapWwd->tileDescriptions[i].insideAttrib);

            ReadRect(inputStream, wapWwd->tileDescriptions[i].rect);
        }
    }
}

WapWwd* WAP_TryWwdLoadFromImpl(char* data, uint32_t length)
{
    WapWwd* wapWwd = NULL;

    if (data == NULL)
    {
        return NULL;
    }

    /********************** WWD HEADER/PROPERTIES **********************/

    wapWwd = new WapWwd;

    // Set default values
    (*wapWwd) = WapWwd();

    InputStream wwdFileStream(data, length);
    wwdFileStream.read(wapWwd->properties.wwdSignature);
    // Signature holds WWD header size, if size doesnt match then it is not
    // supported WWD file
    if (wapWwd->properties.wwdSignature != EXPECTED_HEADER_SIZE)
    {
        delete wapWwd;
        return NULL;
    }

    wwdFileStream.read(wapWwd->properties.null0);
    wwdFileStream.read(wapWwd->properties.flags);
    wwdFileStream.read(wapWwd->properties.null1);
    wwdFileStream.read(wapWwd->properties.levelName);
    wwdFileStream.read(wapWwd->properties.author);
    wwdFileStream.read(wapWwd->properties.birth);
    wwdFileStream.read(wapWwd->properties.rezFile);
    wwdFileStream.read(wapWwd->properties.imageDirectoryPath);
    wwdFileStream.read(wapWwd->properties.rezPalettePath);
    wwdFileStream.read(wapWwd->properties.startX);
    wwdFileStream.read(wapWwd->properties.startY);
    wwdFileStream.read(wapWwd->properties.null2);
    wwdFileStream.read(wapWwd->properties.numPlanes);
    wwdFileStream.read(wapWwd->properties.planesOffset);
    wwdFileStream.read(wapWwd->properties.tileDescriptionsOffset);
    wwdFileStream.read(wapWwd->properties.mainBlockLength);
    wwdFileStream.read(wapWwd->properties.checksum);
    wwdFileStream.read(wapWwd->properties.null3);
    wwdFileStream.read(wapWwd->properties.launchApp);
    wwdFileStream.read(wapWwd->properties.imageSet1);
    wwdFileStream.read(wapWwd->properties.imageSet2);
    wwdFileStream.read(wapWwd->properties.imageSet3);
    wwdFileStream.read(wapWwd->properties.imageSet4);
    wwdFileStream.read(wapWwd->properties.prefix1);
    wwdFileStream.read(wapWwd->properties.prefix2);
    wwdFileStream.read(wapWwd->properties.prefix3);
    wwdFileStream.read(wapWwd->properties.prefix4);

    // Data duplication for sanity reasons
    wapWwd->planesCount = wapWwd->properties.numPlanes;

    // Compressed WWD file payload info
    const char* compressedMainBlock = data + wapWwd->properties.planesOffset;
    size_t compressedMainBlockSize = length - wapWwd->properties.planesOffset;

    // Uncompressed WWD file payload info
    std::vector<char> decompressedMainBlockVector(wapWwd->properties.planesOffset + wapWwd->properties.mainBlockLength);
    memcpy(&decompressedMainBlockVector[0], data, wapWwd->properties.planesOffset);
    char* decompressedMainBlock = &decompressedMainBlockVector[0] + wapWwd->properties.planesOffset;

    // Inflate compressed WWD file payload
    int32_t ret = uncompress((Bytef*)decompressedMainBlock, (uLong*)(&(wapWwd->properties.mainBlockLength)),
        (Bytef*)compressedMainBlock, compressedMainBlockSize);
    // Check if inflation succeeded, if not, free allocated resources and return NULL
    if (ret != Z_OK)
    {
        delete wapWwd;
        return NULL;
    }

    // Create new file stream from inflated WWD file payload
    InputStream wwdFileStreamInflated(&decompressedMainBlockVector[0], decompressedMainBlockVector.size());

    /**************************************************************************************/
    /***************************** READ LEVEL INFORMATIONS ********************************/
    /**************************************************************************************/

    /********************** WWD PLANES **********************/

    ReadPlanes(wapWwd, wwdFileStreamInflated);

    /********************** WWD TILE DESCRIPTIONS **********************/

    ReadTileDescriptions(wapWwd, wwdFileStreamInflated);

    /*******************************************************************/

    return wapWwd;
}

WapWwd* WAP_WwdLoadFromData(char* data, uint32_t length)
{
    try
    {
        return WAP_TryWwdLoadFromImpl(data, length);
    }
    catch (...)
    {
        return NULL;
    }
}

WapWwd* WAP_WwdLoadFromFile(char* wwdFilePath)
{
    std::ifstream wwdFileStream(wwdFilePath, std::ios::binary);
    if (!wwdFileStream.is_open())
    {
        return NULL;
    }

    // Read whole file
    std::vector<char> wwdFileContents((std::istreambuf_iterator<char>(wwdFileStream)), std::istreambuf_iterator<char>());
    if (!wwdFileStream.good())
    {
        return NULL;
    }

    return WAP_WwdLoadFromData((wwdFileContents.empty() ? NULL : &wwdFileContents[0]), wwdFileContents.size());
}

WapWwd* WAP_WwdLoadFromRezFile(RezFile* rezFile)
{
    // Check input validity
    if (rezFile == NULL)
    {
        return NULL;
    }

    char* data = WAP_GetRezFileData(rezFile);
    if (data == NULL)
    {
        return NULL;
    }

    return WAP_WwdLoadFromData(data, rezFile->size);
}

WapWwd* WAP_WwdLoadFromRezArchive(RezArchive* rezArchive, const char* wwdFilePath)
{
    // Check input validity
    if ((rezArchive == NULL) || (wwdFilePath == NULL))
    {
        return NULL;
    }

    RezFile* wwdRezFile = WAP_GetRezFileFromRezArchive(rezArchive, wwdFilePath);
    if (wwdRezFile == NULL)
    {
        return NULL;
    }

    return WAP_WwdLoadFromRezFile(wwdRezFile);
}

RezDirectory* FindRezSubDirectory(RezDirectory* rezDirectory, char* searchedName)
{
    uint32_t i;
    RezDirectory* searchedDirectory = NULL;
    // Check for validity
    if (rezDirectory == NULL || rezDirectory->directoryContents == NULL)
    {
        return NULL;
    }
    // Find folder according to levelName
    for (i = 0; i < rezDirectory->directoryContents->rezDirectoriesCount; i++)
    {
        if (strcmp(searchedName, rezDirectory->directoryContents->rezDirectories[i]->name) == 0)
        {
            searchedDirectory = rezDirectory->directoryContents->rezDirectories[i];
            break;
        }
    }

    return searchedDirectory;
}

RezFile* FindRezFile(RezDirectory* rezDirectory, char* name, char* extension)
{
    uint32_t i;
    RezFile* searchedRezFile = NULL;
    // Check for validity
    if (rezDirectory == NULL || rezDirectory->directoryContents == NULL)
    {
        return NULL;
    }

    // Find folder according to levelName
    for (i = 0; i < rezDirectory->directoryContents->rezFilesCount; i++)
    {
        if ((strcmp(name, rezDirectory->directoryContents->rezFiles[i]->name) == 0) &&
            (strcmp(extension, rezDirectory->directoryContents->rezFiles[i]->extension) == 0))
        {
            searchedRezFile = rezDirectory->directoryContents->rezFiles[i];
            break;
        }
    }

    return searchedRezFile;
}

// Valgrind - "All heap blocks were freed -- no leaks are possible"
void WAP_WwdDestroy(WapWwd* wapWwd)
{
    uint32_t i;

    // Check for sanity
    if (wapWwd == NULL)
    {
        return;
    }

    // Delete planes
    for (i = 0; i < wapWwd->planesCount; i++)
    {
        uint32_t j;

        // Delete plane's image sets
        for (j = 0; j < wapWwd->planes[i].imageSetsCount; j++)
        {
            delete[] wapWwd->planes[i].imageSets[j];
        }
        delete[] wapWwd->planes[i].imageSets;

        // Delete plane's tiles
        delete[] wapWwd->planes[i].tiles;

        // Delete plane's object names
        for (j = 0; j < wapWwd->planes[i].objectsCount; j++)
        {
            delete[] wapWwd->planes[i].objects[j].name;
            delete[] wapWwd->planes[i].objects[j].logic;
            delete[] wapWwd->planes[i].objects[j].imageSet;
            delete[] wapWwd->planes[i].objects[j].sound;
        }

        // Delete plane's objects
        delete[] wapWwd->planes[i].objects;
    }
    // Delete planes itselves
    delete[] wapWwd->planes;

    // Delete tile descriptions
    delete[] wapWwd->tileDescriptions;

    // Delete WAP_WWD
    delete wapWwd;
    wapWwd = NULL;
}