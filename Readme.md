 # This application shows the Clay library introductory video example without using the macro API.
The original example, in C, can be found [here](https://github.com/nicbarker/clay/tree/main/examples/introducing-clay-video-demo).
The differences with this application (aside from being in C++) are: 

1. Allows for `std::strings`, and demostrates changing text dynamically.
2. Encapsulates initialization, updates, and rendering in appropriate functions.
3. Encapsulates configuration calls into simplified functions.
4. No macros from the Clay library are used.
    
If not already, you should first get familiar the the standard way of using the Clay macro API as shown in the original example.

When `CLAY` macro expands, it looks something like this:
 ```cpp
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
```
Which ultimately does this, after the for-loop is unrolled:
```cpp
Clay__OpenElement();
// parameter/configuration expansions here
Clay__ElementPostConfiguration();
//children macro expansions here
Clay__CloseElement();
```
So when nesting clay elements like this:
```cpp
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
```
It expands and unrolls to something like this:
```cpp
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
```

Look at this function for example:
```cpp
//reusable header button
void RenderHeaderButton(Clay_String text) {
    ////////CLAY() macro
    Clay__OpenElement(); 
    // (params)
    applyClayLayoutConfig({ .padding = { 16, 16, 8, 8 }}); 
    applyClayRectangleConfig({
     .color = { 140, 140, 140, 255 },
     .cornerRadius = 5 
    });
    //end (params)
    Clay__ElementPostConfiguration();

    //{children}
    clayTextElement(text, { 
    .fontSize = 16 
    .fontId = FONT_ID_BODY_16, 
    .textColor = { 255, 255, 255, 255 }, 
    });

    //end {children}
    Clay__CloseElement();
}
```
NOTE: `applyClayLayoutConfig`, `applyClayRectangleConfig` and `clayTextElement` are helper functions that contain the expansions of `CLAY_LAYOUT`, `CLAY_RECTANGLE`, `CLAY_TEXT` ( and `CLAY_TEXT_CONFIG`).

The Macro-based version would look like this:
```cpp
void RenderHeaderButton(Clay_String text) {
    CLAY(
        CLAY_LAYOUT({ .padding = { 16, 16, 8, 8 }}),
        CLAY_RECTANGLE({
            .color = { 140, 140, 140, 255 },
            .cornerRadius = 5
        })
    ) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY_16,
            .fontSize = 16,
            .textColor = { 255, 255, 255, 255 }
        }));
    }
}
```

This application is split into a few main parts:
1. Helper Functions - application-independant functions for Clay, and to encapsulate some functionality and expansions.
2. App-Specific Globals - global variables, functions, and reusables specific to the example application.
3. Main Layout Function - a function for encapsulating the entire layout structure, returns `Clay_RenderCommandArray` for rendering.
4. Application - entry into the application (main). Start here.
