#include <iostream>
#include <cstring>
#include <vector>
#include <memory>
#define CLAY_IMPLEMENTATION
#include "./clay.h"
#include "./raylib/clay_renderer_raylib.c"

/*
    This document shows the Clay library introductory video example without using the macro API.
    It also allows for std::strings, and demostrates changing text dynamically.
    It also encapsulates initialization, updates, and rendering in appropriate functions.
    It also encapsulates configuration calls into simplified functions.

    Those familiar with the standard clay library macro API:
    When CLAY macro expands, it looks something like this:
        for ( 
            CLAY__ELEMENT_DEFINITION_LATCH = (
                Clay__OpenElement(), 
                // parameter/configuration expansions here
                Clay__ElementPostConfiguration(), 
                0
            ); 
            CLAY__ELEMENT_DEFINITION_LATCH < 1; 
            ++CLAY__ELEMENT_DEFINITION_LATCH, 
            Clay__CloseElement() 
        
        ){
            //children macro expansions here
        }

    Which ultimately does this:
        Clay__OpenElement();
        // parameter/configuration expansions here
        Clay__ElementPostConfiguration();
        //children macro expansions here
        Clay__CloseElement();

    So when nesting clay elements like this:
        CLAY(
            //params
        ){
            CLAY(
                //params
            ){
                CLAY(
                    //params
                ){}
            }
        }

    It expands to something like this:
        Clay__OpenElement(); //begin macro 1
        //macro 1 parameter/configuration expansions here
        Clay__ElementPostConfiguration(); //macro 1 post config
        //children macro 1 start
        Clay__OpenElement(); //begin macro 2
        //macro 2 parameter/configuration expansions here
        Clay__ElementPostConfiguration(); //macro 1 post config
        //children macro 2 start
        Clay__OpenElement(); //begin macro 3
        //macro 3 parameter/configuration expansions here
        Clay__ElementPostConfiguration(); //macro 1 post config
        Clay__CloseElement(); //macro 3 has no children
        Clay__CloseElement(); //children macro 2 end
        Clay__CloseElement(); //children macro 1 end
*/

////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////helper functions//////////////////////////////////////

//passed to Clay for error handling
void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
}

//called to initialize clay
void initClay(float windowWidth, float windowHeight, Clay_Dimensions (*measureTextFunction)(Clay_String *text, Clay_TextElementConfig *config)){
    uint64_t clayRequiredMemory = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(clayRequiredMemory, malloc(clayRequiredMemory));

    Clay_Initialize(clayMemory, (Clay_Dimensions) {
       .width = windowWidth,
       .height = windowHeight
    }, (Clay_ErrorHandler) { HandleClayErrors });

    Clay_SetMeasureTextFunction(measureTextFunction);
}

//called to initialize Raylib //TODO, allow multiple fonts to be passed
void initRaylib(uint32_t initialWidth, uint32_t initialHeight, const char* title, uint32_t fontIndex, const char* fontPath, uint32_t loadedFontSize){
    Clay_Raylib_Initialize(initialWidth, initialHeight, title, FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    Raylib_fonts[fontIndex] = (Raylib_Font) { 
        .fontId = fontIndex,
        .font = LoadFontEx(fontPath, loadedFontSize, 0, 400)
    };

    SetTextureFilter(Raylib_fonts[fontIndex].font.texture, TEXTURE_FILTER_BILINEAR);
}

//called to render through raylib
void raylibRender(Clay_RenderCommandArray renderCommands){
    BeginDrawing();
    ClearBackground(BLACK);
    Clay_Raylib_Render(renderCommands);
    EndDrawing();
}

//wrapper for layout config calls
void applyClayLayoutConfig(Clay_LayoutConfig layout){
    Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper(layout)).wrapped));
}

//wrapper for rectangle config calls
void applyClayRectangleConfig(Clay_RectangleElementConfig rectangleConfig){
    Clay__AttachElementConfig(
        Clay_ElementConfigUnion { 
            .rectangleElementConfig = Clay__StoreRectangleElementConfig(
                (Clay__Clay_RectangleElementConfigWrapper (rectangleConfig)).wrapped
            ) 
        }, 
        CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE
    );
}

//wrapper for clay ID call
template<size_t N>
constexpr void attachClayID(const char(&str)[N]) {
    Clay_String id = Clay_String{
        .length = static_cast<int32_t>((N - 1)), // Exclude the null terminator
        .chars = str
    };
    Clay__AttachId(Clay__HashString(id, 0, 0));
}

//wrapper for window, mouse, and delta updates to Clay
void updateClayStateInput(float windowWidth, float windowHeight, float mouseX, float mouseY, float scrollDeltaX, float scrollDeltaY, float frameTime, bool leftButtonDown){
    Clay_SetLayoutDimensions((Clay_Dimensions) {
            .width = windowWidth,
            .height = windowHeight
    });

    Clay_SetPointerState(
        (Clay_Vector2) { mouseX, mouseY },
        leftButtonDown
    );

    Clay_UpdateScrollContainers(
        true,
        (Clay_Vector2) { scrollDeltaX, scrollDeltaY },
        frameTime
    );
}

//replacement for CLAY_STRING macro, using literals as input
template<size_t N>
constexpr Clay_String toClayString(const char(&str)[N]) {
    Clay_String toRet = Clay_String{
        .length = static_cast<int32_t>((N - 1)), 
        .chars = str
    };
    
    return toRet;
}

//temporary solution to allow std::strings directly with managed lifetime
//uses vector of pointers to hold conversions from string to char*
std::vector<std::unique_ptr<char[]>> clayStringBuffers;
void clearClayStringBuffers() {
    clayStringBuffers.clear();
}

Clay_String toClayString(const std::string& str) {
    size_t requiredSize = str.size() + 1;
    auto buffer = std::make_unique<char[]>(requiredSize);
    std::strcpy(buffer.get(), str.c_str());

    Clay_String clayStr = {
        .length = static_cast<int32_t>(str.size()),
        .chars = buffer.get()
    };

    clayStringBuffers.push_back(std::move(buffer));
    return clayStr;
}

//wrapper for text element call
void clayTextElement(Clay_String text, Clay_TextElementConfig textElementConfig){
    Clay__OpenTextElement(
        text, 
        Clay__StoreTextElementConfig(
           (Clay__Clay_TextElementConfigWrapper(textElementConfig)).wrapped
        )
    );
}

//////////////////////////////////////////////////////////////////////////////////
////////////////////////////////app-specific//////////////////////////////////////

//debug switch for single-use logging/action in a loop
bool oneshot = false;

//for dynamic testing
uint32_t framecount = 0;

//raylib font index
const int FONT_ID_BODY_16 = 0;

//reusable colors
Clay_Color COLOR_WHITE = { 255, 255, 255, 255};

///////////////////////reusable configs

//reusable sizing configs
Clay_Sizing layoutExpandXY = {
    .width = (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW }),
    .height = (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW })
};

Clay_Sizing layoutExpandX = {
    .width = (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW }),
};

//reusable layout configs
Clay_LayoutConfig layoutElement = Clay_LayoutConfig { .padding = {5, 5, 0, 0} };
Clay_LayoutConfig headerButtonLayoutConfig = Clay_LayoutConfig { .padding = { 16, 16, 8, 8 } };
Clay_LayoutConfig dropdownItemLayoutConfig = Clay_LayoutConfig { .padding = { 16, 16, 16, 16 }};
Clay_LayoutConfig sidebarButtonLayout = {.sizing = layoutExpandX, .padding = { 16, 16, 16, 16 }};

//reusable text configs
Clay_TextElementConfig headerButtonTextConfig = Clay_TextElementConfig { 
    .textColor = { 255, 255, 255, 255 }, 
    .fontId = FONT_ID_BODY_16, 
    .fontSize = 16 
};
Clay_TextElementConfig sidebarButtonTextConfig = Clay_TextElementConfig { 
    .textColor = { 255, 255, 255, 255 }, 
    .fontId = FONT_ID_BODY_16, 
    .fontSize = 20
};
Clay_TextElementConfig documentTextConfig = Clay_TextElementConfig { 
    .textColor = { 255, 255, 255, 255 }, 
    .fontId = FONT_ID_BODY_16, 
    .fontSize = 24
};

//reusable rectangle configs
Clay_RectangleElementConfig contentBackgroundConfig = {
    .color = { 90, 90, 90, 255 },
    .cornerRadius = 8
};

///////////////app globals

//document struct
typedef struct {
    std::string title;
    std::string contents;
} Document;

//document array struct
typedef struct {
    Document *documents;
    uint32_t length;
} DocumentArray;

//global documents array
DocumentArray documents = {
    NULL,
    0
};
uint32_t selectedDocumentIndex = 0;

//callback for on-hover
void HandleSidebarInteraction(
    Clay_ElementId elementId,
    Clay_PointerData pointerData,
    intptr_t userData
) {
    if (pointerData.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) { 
        if (userData >= 0 && userData < documents.length) {
            selectedDocumentIndex = userData;
        }
    }
}


////////////reusable elements

//reusable header button
void RenderHeaderButton(Clay_String text) {
    ////////CLAY() macro
    Clay__OpenElement(); 

    // (params)
    applyClayLayoutConfig(headerButtonLayoutConfig); 
    applyClayRectangleConfig({ .color = { 140, 140, 140, 255 }, .cornerRadius = 5 });

    //end (params)
    Clay__ElementPostConfiguration();

    //{children}
    clayTextElement(text, headerButtonTextConfig);

    //end {children}
    Clay__CloseElement();
}

//reusable dropdown item //TODO, find a way to safely pass std::string instead of Clay_String (layout glitches)
void RenderDropdownMenuItem(Clay_String text) {
    ////////CLAY() macro
    Clay__OpenElement();

    // (params)
    applyClayLayoutConfig(dropdownItemLayoutConfig);

    //end (params)
    Clay__ElementPostConfiguration();

    //{children}
    clayTextElement(text, headerButtonTextConfig);

    //end {children}
    Clay__CloseElement();
}

//main layout function call
Clay_RenderCommandArray buildLayout(){
    Clay_BeginLayout(); //START LAYOUT

    Clay__OpenElement(), //macro 1 (has been converted from for loop to linear, close element call is at end of function)
    
    //macro 1 params, TODO element config function and use of layout config function
    attachClayID("OuterContainer");
    Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { { .color = { 43, 41, 51, 255 } } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE);
    Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = layoutExpandXY, .padding = { 16, 16, 16, 16 }, .childGap = 16, .layoutDirection = CLAY_TOP_TO_BOTTOM } }).wrapped));
    Clay__ElementPostConfiguration(); 

    //macro 1 children
    for ( //macro 2, child of macro 1 //TODO convert for loop to linear
        CLAY__ELEMENT_DEFINITION_LATCH = (
            Clay__OpenElement(), 
            attachClayID("HeaderBar"), 
            Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { {contentBackgroundConfig} }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
            Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = { .width = (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW }), .height = (Clay_SizingAxis { .size = { .minMax = { 60, 60 } }, .type = CLAY__SIZING_TYPE_FIXED }) }, .padding = { 16, 16, 0, 0 }, .childGap = 16, .childAlignment = { .y = CLAY_ALIGN_Y_CENTER } } }).wrapped)), 
            Clay__ElementPostConfiguration(), 
            0
        ); 
        CLAY__ELEMENT_DEFINITION_LATCH < 1; 
        ++CLAY__ELEMENT_DEFINITION_LATCH, 
        Clay__CloseElement() 
    
    ){ //macro 2 children
        for ( //macro 4, child of macro 2 //TODO convert for loop to linear
            CLAY__ELEMENT_DEFINITION_LATCH = (
                Clay__OpenElement(), 
                attachClayID("FileButton"), 
                Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .padding = { 16, 16, 8, 8 }} }).wrapped)), 
                Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { { .color = { 140, 140, 140, 255 }, .cornerRadius = 5 } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
                Clay__ElementPostConfiguration(), 
                0
            ); 
            CLAY__ELEMENT_DEFINITION_LATCH < 1; 
            ++CLAY__ELEMENT_DEFINITION_LATCH, 
            Clay__CloseElement() 
        
        ) { //macro 4 children
            //call to text macro function
            clayTextElement(toClayString("File"),headerButtonTextConfig);

            bool fileMenuVisible = Clay_PointerOver(Clay_GetElementId(toClayString("FileButton"))) || Clay_PointerOver(Clay_GetElementId(toClayString("FileMenu")));
            
            if (fileMenuVisible) {
                for ( //macro 6, child of macro 4 //TODO convert for loop to linear
                    CLAY__ELEMENT_DEFINITION_LATCH = (
                        Clay__OpenElement(), 
                        attachClayID("FileMenu"), 
                        Clay__AttachElementConfig(Clay_ElementConfigUnion { .floatingElementConfig = Clay__StoreFloatingElementConfig((Clay__Clay_FloatingElementConfigWrapper { { .attachment = { .parent = CLAY_ATTACH_POINT_LEFT_BOTTOM }, } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_FLOATING_CONTAINER), 
                        Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .padding = {0, 0, 8, 8 } } }).wrapped)), 
                        Clay__ElementPostConfiguration(), 
                        0
                    ); 
                    CLAY__ELEMENT_DEFINITION_LATCH < 1; 
                    ++CLAY__ELEMENT_DEFINITION_LATCH, 
                    Clay__CloseElement() 
                
                ) { //macro 6 children
                    for ( //macro 7, child of macro 6 //TODO convert for loop to linear
                        CLAY__ELEMENT_DEFINITION_LATCH = (
                            Clay__OpenElement(), 
                            Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = { .width = (Clay_SizingAxis { .size = { .minMax = { 200, 200 } }, .type = CLAY__SIZING_TYPE_FIXED }) }, .layoutDirection = CLAY_TOP_TO_BOTTOM } }).wrapped)), 
                            Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { { .color = { 40, 40, 40, 255 }, .cornerRadius = 8 } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
                            Clay__ElementPostConfiguration(), 
                            0
                        ); 
                        CLAY__ELEMENT_DEFINITION_LATCH < 1; 
                        ++CLAY__ELEMENT_DEFINITION_LATCH, 
                        Clay__CloseElement() 
                    
                    ) { //macro 7 children

                        //call reusables
                        RenderDropdownMenuItem(toClayString("New"));
                        RenderDropdownMenuItem(toClayString("Open"));
                        RenderDropdownMenuItem(toClayString("Close"));
                    }
                }
            }
        }

        //call reusable
        RenderHeaderButton(toClayString("Edit"));

        for ( //macro 5, child of macro 2 //TODO convert for loop to linear
            CLAY__ELEMENT_DEFINITION_LATCH = (
                Clay__OpenElement(), 
                Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = { (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW }) }} }).wrapped)), 
                Clay__ElementPostConfiguration(), 
                0
            ); 
            CLAY__ELEMENT_DEFINITION_LATCH < 1; 
            ++CLAY__ELEMENT_DEFINITION_LATCH, 
            Clay__CloseElement() 
        
        ) {
            //macro 5 has no children
        }

        //call reusable elements (children of macro 2)
        RenderHeaderButton(toClayString("Upload"));
        RenderHeaderButton(toClayString("Media"));
        RenderHeaderButton(toClayString("Support"));
    }

    for ( //macro 3, child of macro 1 //TODO convert for loop to linear
        CLAY__ELEMENT_DEFINITION_LATCH = (
            Clay__OpenElement(), 
            attachClayID("LowerContent"), 
            Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = layoutExpandXY, .childGap = 16 } }).wrapped)), 
            Clay__ElementPostConfiguration(), 
            0
        ); 
        CLAY__ELEMENT_DEFINITION_LATCH < 1; 
        ++CLAY__ELEMENT_DEFINITION_LATCH, 
        Clay__CloseElement() 
    
    ) { //children of macro 3
        for ( //macro 8, child of macro 3 //TODO convert for loop to linear
            CLAY__ELEMENT_DEFINITION_LATCH = (
                Clay__OpenElement(), 
                attachClayID("Sidebar"), 
                Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { {contentBackgroundConfig} }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
                Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { { .sizing = { .width = (Clay_SizingAxis { .size = { .minMax = { 250, 250 } }, .type = CLAY__SIZING_TYPE_FIXED }), .height = (Clay_SizingAxis { .size = { .minMax = { {0} } }, .type = CLAY__SIZING_TYPE_GROW }) }, .padding = { 16, 16, 16, 16 }, .childGap = 8, .layoutDirection = CLAY_TOP_TO_BOTTOM } }).wrapped)), 
                Clay__ElementPostConfiguration(), 
                0
            ); 
            CLAY__ELEMENT_DEFINITION_LATCH < 1; 
            ++CLAY__ELEMENT_DEFINITION_LATCH, 
            Clay__CloseElement() 
        
        ) { //children of macro 8
            //sidebar item loop
            for (int i = 0; i < documents.length; i++) {
                if (i == selectedDocumentIndex) {
                    for ( //macro 9, child of macro 8 //TODO convert for loop to linear
                        CLAY__ELEMENT_DEFINITION_LATCH = (
                            Clay__OpenElement(), 
                            Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { sidebarButtonLayout }).wrapped)), 
                            Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { { .color = { 120, 120, 120, 255 }, .cornerRadius = 8, } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
                            Clay__ElementPostConfiguration(), 
                            0
                        ); 
                        CLAY__ELEMENT_DEFINITION_LATCH < 1; 
                        ++CLAY__ELEMENT_DEFINITION_LATCH,
                        Clay__CloseElement() 
                    
                    ){ //children of macro 9
                        std::string str = documents.documents[i].title;
                        clayTextElement(toClayString(str),sidebarButtonTextConfig);
                    }
                } else {
                    for ( //macro 10, child of macro 8 //TODO convert for loop to linear
                        CLAY__ELEMENT_DEFINITION_LATCH = (
                            Clay__OpenElement(), 
                            Clay__AttachLayoutConfig(Clay__StoreLayoutConfig((Clay__Clay_LayoutConfigWrapper { sidebarButtonLayout }).wrapped)), 
                            Clay_OnHover(HandleSidebarInteraction, i), 
                            Clay_Hovered()? 
                                Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { { .color = { 120, 120, 120, 120 }, .cornerRadius = 8 } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE) 
                                : (void)0, 
                            Clay__ElementPostConfiguration(), 
                            0
                        ); 
                        CLAY__ELEMENT_DEFINITION_LATCH < 1; 
                        ++CLAY__ELEMENT_DEFINITION_LATCH, 
                        Clay__CloseElement() 
                    ) { //macro 10 children
                        std::string str = documents.documents[i].title;
                        clayTextElement(toClayString(str), sidebarButtonTextConfig);
                    }
                }
            }
        }

        for ( //macro 11, child of macro 3 //TODO convert for loop to linear
            CLAY__ELEMENT_DEFINITION_LATCH = (
                Clay__OpenElement(), 
                attachClayID("MainContent"), 
                Clay__AttachElementConfig(Clay_ElementConfigUnion { .rectangleElementConfig = Clay__StoreRectangleElementConfig((Clay__Clay_RectangleElementConfigWrapper { {contentBackgroundConfig} }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_RECTANGLE), 
                Clay__AttachElementConfig(Clay_ElementConfigUnion { .scrollElementConfig = Clay__StoreScrollElementConfig((Clay__Clay_ScrollElementConfigWrapper { { .vertical = true } }).wrapped) }, CLAY__ELEMENT_CONFIG_TYPE_SCROLL_CONTAINER), 
                Clay__AttachLayoutConfig( Clay__StoreLayoutConfig( ( Clay__Clay_LayoutConfigWrapper { { .sizing = layoutExpandXY, .padding = { 16, 16, 16, 16 }, .childGap = 16, .layoutDirection = CLAY_TOP_TO_BOTTOM } } ).wrapped ) ), 
                Clay__ElementPostConfiguration(), 
                0
            ); 
            CLAY__ELEMENT_DEFINITION_LATCH < 1; 
            ++CLAY__ELEMENT_DEFINITION_LATCH, 
            Clay__CloseElement() 
        
        ){ //children of macro 11
            clayTextElement(toClayString(documents.documents[selectedDocumentIndex].title), documentTextConfig);
            clayTextElement(toClayString(documents.documents[selectedDocumentIndex].contents), documentTextConfig);
        }
    }
    
    //end macro 1 children
    Clay__CloseElement();

    return Clay_EndLayout(); //END LAYOUT
}

/////////////////////////////////////////////////////////////////////
///////////////////////begin application/////////////////////////////
int main(void) {

    //initialize Clay and Raylib
    initRaylib(1024, 768, "Clay C++ No Macros Introductory Video Example", FONT_ID_BODY_16, "resources/Roboto-Regular.ttf", 48);
    initClay((float)GetScreenWidth(), (float)GetScreenHeight(), Raylib_MeasureText);

    //define initial data
    Document docs[] = {
        {"Squirrels", "The Secret Life of Squirrels: Nature's Clever Acrobats\nSquirrels are often overlooked creatures, dismissed as mere park inhabitants or backyard nuisances. Yet, beneath their fluffy tails and twitching noses lies an intricate world of cunning, agility, and survival tactics that are nothing short of fascinating. As one of the most common mammals in North America, squirrels have adapted to a wide range of environments from bustling urban centers to tranquil forests and have developed a variety of unique behaviors that continue to intrigue scientists and nature enthusiasts alike.\n\nMaster Tree Climbers\nAt the heart of a squirrel's skill set is its impressive ability to navigate trees with ease. Whether they're darting from branch to branch or leaping across wide gaps, squirrels possess an innate talent for acrobatics. Their powerful hind legs, which are longer than their front legs, give them remarkable jumping power. With a tail that acts as a counterbalance, squirrels can leap distances of up to ten times the length of their body, making them some of the best aerial acrobats in the animal kingdom.\nBut it's not just their agility that makes them exceptional climbers. Squirrels' sharp, curved claws allow them to grip tree bark with precision, while the soft pads on their feet provide traction on slippery surfaces. Their ability to run at high speeds and scale vertical trunks with ease is a testament to the evolutionary adaptations that have made them so successful in their arboreal habitats.\n\nFood Hoarders Extraordinaire\nSquirrels are often seen frantically gathering nuts, seeds, and even fungi in preparation for winter. While this behavior may seem like instinctual hoarding, it is actually a survival strategy that has been honed over millions of years. Known as \"scatter hoarding,\" squirrels store their food in a variety of hidden locations, often burying it deep in the soil or stashing it in hollowed-out tree trunks.\nInterestingly, squirrels have an incredible memory for the locations of their caches. Research has shown that they can remember thousands of hiding spots, often returning to them months later when food is scarce. However, they don't always recover every stash some forgotten caches eventually sprout into new trees, contributing to forest regeneration. This unintentional role as forest gardeners highlights the ecological importance of squirrels in their ecosystems.\n\nThe Great Squirrel Debate: Urban vs. Wild\nWhile squirrels are most commonly associated with rural or wooded areas, their adaptability has allowed them to thrive in urban environments as well. In cities, squirrels have become adept at finding food sources in places like parks, streets, and even garbage cans. However, their urban counterparts face unique challenges, including traffic, predators, and the lack of natural shelters. Despite these obstacles, squirrels in urban areas are often observed using human infrastructure such as buildings, bridges, and power lines as highways for their acrobatic escapades.\nThere is, however, a growing concern regarding the impact of urban life on squirrel populations. Pollution, deforestation, and the loss of natural habitats are making it more difficult for squirrels to find adequate food and shelter. As a result, conservationists are focusing on creating squirrel-friendly spaces within cities, with the goal of ensuring these resourceful creatures continue to thrive in both rural and urban landscapes.\n\nA Symbol of Resilience\nIn many cultures, squirrels are symbols of resourcefulness, adaptability, and preparation. Their ability to thrive in a variety of environments while navigating challenges with agility and grace serves as a reminder of the resilience inherent in nature. Whether you encounter them in a quiet forest, a city park, or your own backyard, squirrels are creatures that never fail to amaze with their endless energy and ingenuity.\nIn the end, squirrels may be small, but they are mighty in their ability to survive and thrive in a world that is constantly changing. So next time you spot one hopping across a branch or darting across your lawn, take a moment to appreciate the remarkable acrobat at work a true marvel of the natural world.\n" },
        {"Lorem Ipsum", "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum." },
        {"Vacuum Instructions", "Chapter 3: Getting Started - Unpacking and Setup\n\nCongratulations on your new SuperClean Pro 5000 vacuum cleaner! In this section, we will guide you through the simple steps to get your vacuum up and running. Before you begin, please ensure that you have all the components listed in the \"Package Contents\" section on page 2.\n\n1. Unboxing Your Vacuum\nCarefully remove the vacuum cleaner from the box. Avoid using sharp objects that could damage the product. Once removed, place the unit on a flat, stable surface to proceed with the setup. Inside the box, you should find:\n\n    The main vacuum unit\n    A telescoping extension wand\n    A set of specialized cleaning tools (crevice tool, upholstery brush, etc.)\n    A reusable dust bag (if applicable)\n    A power cord with a 3-prong plug\n    A set of quick-start instructions\n\n2. Assembling Your Vacuum\nBegin by attaching the extension wand to the main body of the vacuum cleaner. Line up the connectors and twist the wand into place until you hear a click. Next, select the desired cleaning tool and firmly attach it to the wand's end, ensuring it is securely locked in.\n\nFor models that require a dust bag, slide the bag into the compartment at the back of the vacuum, making sure it is properly aligned with the internal mechanism. If your vacuum uses a bagless system, ensure the dust container is correctly seated and locked in place before use.\n\n3. Powering On\nTo start the vacuum, plug the power cord into a grounded electrical outlet. Once plugged in, locate the power switch, usually positioned on the side of the handle or body of the unit, depending on your model. Press the switch to the \"On\" position, and you should hear the motor begin to hum. If the vacuum does not power on, check that the power cord is securely plugged in, and ensure there are no blockages in the power switch.\n\nNote: Before first use, ensure that the vacuum filter (if your model has one) is properly installed. If unsure, refer to \"Section 5: Maintenance\" for filter installation instructions." },
        {"Article 4", "Article 4" },
        {"Article 5", "Article 5" }
    };
    documents.documents = docs;
    documents.length = 5;

    /////////////////////begin application loop
    while (!WindowShouldClose()) {

        //update Clay state
        Vector2 mousePosition = GetMousePosition();
        Vector2 scrollDelta = GetMouseWheelMoveV();
        updateClayStateInput((float)GetScreenWidth(), (float)GetScreenHeight(), mousePosition.x, mousePosition.y, scrollDelta.x, scrollDelta.y, GetFrameTime(), IsMouseButtonDown(0));

        

        //build and render layout
        Clay_RenderCommandArray renderCommands = buildLayout(); 
        raylibRender(renderCommands);

        //clear string buffer cache (delete pointers)
        clearClayStringBuffers();

        //increment frame count, reset at maximum
        if(framecount < std::numeric_limits<uint32_t>::max()){
            framecount++;
        }else{
            framecount = 0;
        }

        if(framecount == 200){
            docs[2] = {"Change-up", "Testing the dynamic ability of strings with C++ magic!"};
        }else if(framecount == 400){ 
            //TODO add new document dynamically, may require vector...
        }
    }
    return 0;
}