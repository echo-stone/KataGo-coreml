import Foundation
import MetalPerformanceShaders
import MetalPerformanceShadersGraph

extension UnsafeMutablePointer<Float32> {
    func printAsFloat() {
        print("data[0]=\(self[0])")
        print("data[1]=\(self[1])")
        print("data[2]=\(self[2])")
        print("data[3]=\(self[3])")
        print("data[4]=\(self[4])")
    }
}

@objc
class ConvLayer: NSObject {
    let graph: MPSGraph
    let sourceTensor: MPSGraphTensor
    let sourceTensorData: MPSGraphTensorData
    let weightsTensor: MPSGraphTensor
    let weightsTensorData: MPSGraphTensorData
    let resultTensor: MPSGraphTensor

    @objc
    class func test(convXSize: NSNumber,
                    convYSize: NSNumber,
                    inChannels: NSNumber,
                    outChannels: NSNumber,
                    dilationX: NSNumber,
                    dilationY: NSNumber,
                    nnXLen: NSNumber,
                    nnYLen: NSNumber,
                    batchSize: NSNumber,
                    useFB16: NSNumber,
                    useNHWC: NSNumber,
                    weights: UnsafeMutablePointer<Float32>,
                    input: UnsafeMutablePointer<Float32>,
                    output: UnsafeMutablePointer<Float32>) {
        let device = MPSGraphDevice(mtlDevice: MTLCreateSystemDefaultDevice()!)

        let layer = ConvLayer(device: device,
                              graph: MPSGraph(),
                              convXSize: convXSize,
                              convYSize: convYSize,
                              inChannels: inChannels,
                              outChannels: outChannels,
                              dilationX: dilationX,
                              dilationY: dilationY,
                              nnXLen: nnXLen,
                              nnYLen: nnYLen,
                              weights: weights)

        let numInputElements = inChannels.intValue * nnYLen.intValue * nnXLen.intValue
        let numOutputElements = outChannels.intValue * nnYLen.intValue * nnXLen.intValue

        for i in 0..<batchSize.intValue {
            let convInput = input + (i * numInputElements)
            let convOutput = output + (i * numOutputElements)
            layer.apply(input: convInput, output: convOutput)
        }
    }

    init(device: MPSGraphDevice,
         graph: MPSGraph,
         convXSize: NSNumber,
         convYSize: NSNumber,
         inChannels: NSNumber,
         outChannels: NSNumber,
         dilationX: NSNumber,
         dilationY: NSNumber,
         nnXLen: NSNumber,
         nnYLen: NSNumber,
         weights: UnsafeMutablePointer<Float32>) {
        self.graph = graph

        let sourceShape = [1,
                           inChannels,
                           nnYLen.intValue as NSNumber,
                           nnXLen.intValue as NSNumber]

        sourceTensor = graph.placeholder(shape: sourceShape,
                                         name: nil)

        let sourceDescriptor = MPSNDArrayDescriptor(dataType: sourceTensor.dataType,
                                                    shape: sourceTensor.shape!)

        let sourceArray = MPSNDArray(device: device.metalDevice!, descriptor: sourceDescriptor)

        sourceTensorData = MPSGraphTensorData(sourceArray)

        let weightsShape = [outChannels,
                            inChannels,
                            convYSize,
                            convXSize]

        weightsTensor = graph.placeholder(shape: weightsShape,
                                          name: nil)

        let weightsDescriptor = MPSNDArrayDescriptor(dataType: weightsTensor.dataType,
                                                     shape: weightsTensor.shape!)

        let weightsArray = MPSNDArray(device: device.metalDevice!, descriptor: weightsDescriptor)

        weightsArray.writeBytes(weights, strideBytes: nil)
        weightsTensorData = MPSGraphTensorData(weightsArray)

        let convDescriptor = MPSGraphConvolution2DOpDescriptor(strideInX: 1,
                                                               strideInY: 1,
                                                               dilationRateInX: dilationX.intValue,
                                                               dilationRateInY: dilationY.intValue,
                                                               groups: 1,
                                                               paddingStyle: .explicit,
                                                               dataLayout: .NCHW,
                                                               weightsLayout: .OIHW)!

        resultTensor = graph.convolution2D(sourceTensor,
                                           weights: weightsTensor,
                                           descriptor: convDescriptor,
                                           name: nil)
    }

    func apply(input: UnsafeMutablePointer<Float32>,
               output: UnsafeMutablePointer<Float32>) {
        sourceTensorData.mpsndarray().writeBytes(input, strideBytes: nil)

        let fetch = graph.run(feeds: [sourceTensor: sourceTensorData,
                                     weightsTensor: weightsTensorData],
                              targetTensors: [resultTensor],
                              targetOperations: nil)

        fetch[resultTensor]?.mpsndarray().readBytes(output, strideBytes: nil)
    }
}

@objc
class KataGoGraph: NSObject {
    static let graphs = NSMutableDictionary(capacity: 1)
    let nnXLen: NSNumber
    let nnYLen: NSNumber
    let numInputChannels: NSNumber
    let numInputGlobalChannels: NSNumber
    let device: MTLDevice
    let graph: MPSGraph
    let inputTensor: MPSGraphTensor
    let inputGlobalTensor: MPSGraphTensor
    let symmetriesTensor: MPSGraphTensor
    let includeHistoryTensor: MPSGraphTensor
    let policyOutputTensor: MPSGraphTensor
    let inputTensorData: MPSGraphTensorData
    let inputGlobalTensorData: MPSGraphTensorData

    @objc
    class func getGraph(gpuIndex: NSNumber) -> KataGoGraph {
        return graphs[gpuIndex]! as! KataGoGraph
    }

    @objc
    class func initGraph(gpuIndex: NSNumber,
                         nnXLen: NSNumber,
                         nnYLen: NSNumber,
                         version: NSNumber,
                         numInputChannels: NSNumber,
                         numInputGlobalChannels: NSNumber,
                         numValueChannels: NSNumber,
                         numScoreValueChannels: NSNumber,
                         numOwnershipChannels: NSNumber) {
        objc_sync_enter(self)
        defer { objc_sync_exit(self) }

        if (graphs[gpuIndex] == nil) {
            graphs[gpuIndex] = KataGoGraph(gpuIndex: gpuIndex,
                                           nnXLen: nnXLen,
                                           nnYLen: nnYLen,
                                           version: version,
                                           numInputChannels: numInputChannels,
                                           numInputGlobalChannels: numInputGlobalChannels,
                                           numValueChannels: numValueChannels,
                                           numScoreValueChannels: numScoreValueChannels,
                                           numOwnershipChannels: numOwnershipChannels)
        }
    }

    private init(gpuIndex: NSNumber,
                 nnXLen: NSNumber,
                 nnYLen: NSNumber,
                 version: NSNumber,
                 numInputChannels: NSNumber,
                 numInputGlobalChannels: NSNumber,
                 numValueChannels: NSNumber,
                 numScoreValueChannels: NSNumber,
                 numOwnershipChannels: NSNumber) {
        device = MTLCreateSystemDefaultDevice()!
        self.nnXLen = nnXLen
        self.nnYLen = nnYLen
        self.numInputChannels = numInputChannels
        self.numInputGlobalChannels = numInputGlobalChannels
        graph = MPSGraph()

        inputTensor = graph.placeholder(shape: [nnXLen.intValue as NSNumber,
                                                nnYLen.intValue as NSNumber,
                                                numInputChannels.intValue as NSNumber],
                                        name: "binInputs")

        let inputArrayDesc = MPSNDArrayDescriptor(dataType: inputTensor.dataType,
                                                  shape: inputTensor.shape!)

        let inputArray = MPSNDArray(device: device, descriptor: inputArrayDesc)

        inputTensorData = MPSGraphTensorData(inputArray)

        inputGlobalTensor = graph.placeholder(shape: [numInputGlobalChannels.intValue as NSNumber],
                                              name: "globalInputs")

        let inputGlobalArrayDesc = MPSNDArrayDescriptor(dataType: inputGlobalTensor.dataType,
                                                        shape: inputGlobalTensor.shape!)

        let inputGlobalArray = MPSNDArray(device: device, descriptor: inputGlobalArrayDesc)

        inputGlobalTensorData = MPSGraphTensorData(inputGlobalArray)

        symmetriesTensor = graph.constant(0.0, shape: [3], dataType: .float32)
        includeHistoryTensor = graph.constant(1.0, shape: [5], dataType: .float32)

        // Test
        let numInputElements = NSNumber(integerLiteral: nnXLen.intValue * nnYLen.intValue * numInputChannels.intValue)

        let reshaped = graph.reshape(inputTensor,
                                     shape: [1, numInputElements],
                                     name: nil)

        let weightTensor = graph.constant(1.0,
                                          shape: [numInputElements, 1],
                                          dataType: .float32)

        policyOutputTensor = graph.matrixMultiplication(primary: reshaped,
                                                        secondary: weightTensor,
                                                        name: nil)
    }

    @objc
    func run(userInputBuffer: UnsafeMutablePointer<Float32>,
             userInputGlobalBuffer: UnsafeMutablePointer<Float32>,
             policyOutput: UnsafeMutablePointer<Float32>,
             valueOutput: UnsafeMutablePointer<Float32>,
             ownershipOutput: UnsafeMutablePointer<Float32>,
             miscValuesOutput: UnsafeMutablePointer<Float32>,
             moreMiscValuesOutput: UnsafeMutablePointer<Float32>) {
        let feeds = [inputTensor: inputTensorData,
               inputGlobalTensor: inputGlobalTensorData]

        inputTensorData.mpsndarray().writeBytes(userInputBuffer, strideBytes: nil)
        inputGlobalTensorData.mpsndarray().writeBytes(userInputGlobalBuffer, strideBytes: nil)

        let fetch = graph.run(feeds: feeds,
                              targetTensors: [policyOutputTensor],
                              targetOperations: nil)

        fetch[policyOutputTensor]!.mpsndarray().readBytes(policyOutput, strideBytes: nil)

        // debug
        policyOutput.printAsFloat()
    }
}
