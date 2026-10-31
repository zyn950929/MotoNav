let bikeRequest=MotorRequest(start:MotorPoint(lat:30,lng:104),end:MotorPoint(lat:30.004,lng:104.004),bicycle:true)
let fullBike="104,30;104.004,30;104.004,30.004"
let shapeOnly=try BicycleRouteParser.parse(.init(polyline:fullBike,distance:900,duration:240,segments:[]),request:bikeRequest)
precondition(shapeOnly.points.count==3 && shapeOnly.points[1].lat==30 && shapeOnly.points[1].lng==104.004)
precondition(!shapeOnly.detailedGuidance && shapeOnly.steps.count==1 && shapeOnly.steps[0].end==2)
precondition(shapeOnly.seconds==240 && shapeOnly.totalMeters==900 && shapeOnly.request.bicycle)
let noStepCoordinates=try BicycleRouteParser.parse(.init(polyline:fullBike,distance:900,duration:240,segments:[.init(polyline:"",instruction:"右转",road:"测试道路",action:"右转",distance:900,duration:240)]),request:bikeRequest)
precondition(noStepCoordinates.points.count==3 && !noStepCoordinates.detailedGuidance)
precondition(noStepCoordinates.steps[0].action.isEmpty,"Do not attach a turn instruction to a guessed position")
let normalBike=try BicycleRouteParser.parse(.init(polyline:"",distance:900,duration:240,segments:[
    .init(polyline:"104,30;104.004,30",instruction:"前方右转",road:"甲路",action:"右转",distance:400,duration:100),
    .init(polyline:"104.004,30;104.004,30.004",instruction:"到达目的地",road:"乙路",action:"到达",distance:500,duration:140)
]),request:bikeRequest)
precondition(normalBike.detailedGuidance && normalBike.points.count==3 && normalBike.steps.count==2)
precondition(normalBike.steps[0].dir=="R" && normalBike.steps[1].start==1 && normalBike.steps[1].end==2)
let partialBike=try BicycleRouteParser.parse(.init(polyline:fullBike,distance:900,duration:240,segments:[
    .init(polyline:"104,30;104.004,30",instruction:"前方右转",road:"甲路",action:"右转",distance:400,duration:100),
    .init(polyline:"",instruction:"到达",road:"乙路",action:"",distance:500,duration:140)
]),request:bikeRequest)
precondition(partialBike.points.last?.lat==30.004 && !partialBike.detailedGuidance,"Incomplete step geometry must not truncate the route")
for invalid in ["", "104,30", "104,30;104,30", "104,30;NaN,30.004", "104,30;200,30", "104,30;120,40"] {
    expectFailure{_ = try BicycleRouteParser.parse(.init(polyline:invalid,distance:900,duration:240,segments:[]),request:bikeRequest)}
}
print("PASS SDK bicycle full-path fallback, empty/partial steps, real turn metadata and malformed geometry rejection")
