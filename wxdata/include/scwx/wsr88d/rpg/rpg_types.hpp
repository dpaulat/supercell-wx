#pragma once

namespace scwx
{
namespace wsr88d
{
namespace rpg
{

enum class PacketCode : std::uint16_t
{
   TextNoValue                          = 1,
   SpecialSymbol                        = 2,
   MesocycloneSymbol3                   = 3,
   WindBarbData                         = 4,
   VectorArrowData                      = 5,
   LinkedVectorNoValue                  = 6,
   UnlinkedVectorNoValue                = 7,
   TextUniform                          = 8,
   LinkedVectorUniform                  = 9,
   UnlinkedVectorUniform                = 10,
   MesocycloneSymbol11                  = 11,
   TornadoVortexSignatureSymbol         = 12,
   HailPositiveSymbol                   = 13,
   HailProbableSymbol                   = 14,
   StormId                              = 15,
   DigitalRadialDataArray               = 16,
   DigitalPrecipitationDataArray        = 17,
   PrecipitationRateDataArray           = 18,
   HdaHailSymbol                        = 19,
   PointFeatureSymbol                   = 20,
   CellTrendData                        = 21,
   CellTrendVolumeScanTimes             = 22,
   ScitPastData                         = 23,
   ScitForecastData                     = 24,
   StiCircle                            = 25,
   ElevatedTornadoVortexSignatureSymbol = 26,
   GenericData28                        = 28,
   GenericData29                        = 29,
   SetColorLevel                        = 0x0802,
   LinkedContourVector                  = 0x0E03,
   UnlinkedContourVector                = 0x3501,
   MapMessage0E23                       = 0x0E23,
   MapMessage3521                       = 0x3521,
   MapMessage4E00                       = 0x4E00,
   MapMessage4E01                       = 0x4E01,
   RadialData                           = 0xAF1F,
   RasterDataBA07                       = 0xBA07,
   RasterDataBA0F                       = 0xBA0F
};

enum class SpecialSymbol
{
   PastStormCellPosition,
   CurrentStormCellPosition,
   ForecastStormCellPosition,
   PastMdaPosition,
   ForecastMdaPosition,
   None
};

} // namespace rpg
} // namespace wsr88d
} // namespace scwx
