#ifndef FLAGS_H
#define FLAGS_H 

 //! Bit masks to indicate what type of variables are to be supported by
 //! a particular VariablesWidget instance. These flags correspond
 //! to variable names returned by methods:
 //!
 //! SCALAR : RenderParams::GetVariableName()
 //! VECTOR : RenderParams::GetFieldVariableNames()
 //! HGT : RenderParams::GetHeightVariableName()
 //! COLOR : RenderParams::GetColorMapVariableNames()
 //!

enum VariableFlags {
	SCALAR = (1u << 0),
	VECTOR = (1u << 1),
	COLOR = (1u << 2),
	AUXILIARY = (1u << 3),
	HEIGHT = (1u << 4),
};

 //! Bit mask to indicate whether 2D, 3D, or 2D and 3D variables are to
 //! be supported
 //
enum DimFlags {
	TWOD = (1u << 0),
	THREED = (1u << 1),
};

 //! Bit mask to indicate whether the GeometryWidget should control a
 //! single point, or 3D extents with Min/Max controllers
enum GeometryFlags {
	SINGLEPOINT = (1u << 0),
	MINMAX = (1u << 1),
};

 //! Bit masks to indicate whether the TFWidget maps constant color
 //! to a variable, or maps a secondary color variable
enum TFFlags {
	SECONDARY = (1u << 0),
	CONSTANT = (1u << 1),
};

#endif
