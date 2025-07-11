/*****************************************************************************************
 * SGCT                                                                                  *
 * Simple Graphics Cluster Toolkit                                                       *
 *                                                                                       *
 * Copyright (c) 2012-2025                                                               *
 * For conditions of distribution and use, see copyright notice in LICENSE.md            *
 ****************************************************************************************/

#ifndef __SGCT__ENGINE__H__
#define __SGCT__ENGINE__H__

#include <sgct/sgctexports.h>
#include <sgct/actions.h>
#include <sgct/callbackdata.h>
#include <sgct/config.h>
#include <sgct/definitions.h>
#include <sgct/joystick.h>
#include <sgct/keys.h>
#include <sgct/modifiers.h>
#include <sgct/mouse.h>
#include <sgct/window.h>
#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <thread>

namespace sgct {

struct Configuration;
class Node;
class StatisticsRenderer;

/**
 * Loads the cluster information from the provided \p path. The \p path is a configuration
 * file and should be an absolute path or relative to the current working directory. If no
 * path is provided, a default setup consisting of a FOV-based rendering with a 1280x720
 * window with is loaded instead.
 *
 * \param path The path to the configuration that should be loaded
 * \return The loaded Cluster object that contains all of the information from the file
 *
 * \exception std::runtime_error This exception is thrown whenever an unrecoverable error
 *            occurs while trying to load the provided path. This error is never raised
 *            when providing no path
 * \pre The \p path, if it is provided, must be an existing file
 */
SGCT_EXPORT config::Cluster loadCluster(
    std::optional<std::filesystem::path> path = std::nullopt);

/**
 * Returns the number of seconds since the program start. The resultion of this counter is
 * usually the best available counter from the operating system.
 *
 * \return The number of seconds since the program started
 */
SGCT_EXPORT double time();


/**
 * The Engine class is the central part of SGCT and handles most of the callbacks,
 * rendering, network handling, input devices, etc.
 */
class SGCT_EXPORT Engine {
public:
    using DrawFunction = void (*)(const RenderData&);

    /**
     * Structure with all statistics gathered about different frametimes. The newest value
     * is always at the front of the different arrays, the remaining values being sorted
     * by the frame in which they occured. These values are only collected while the
     * statistics are being shown.
     */
    struct SGCT_EXPORT Statistics {
        /// For how many frames are the history values collected before the oldest values
        /// are replaced
        static constexpr int HistoryLength = 128;

        /// The times that contain the entire time spending processing the frames
        std::array<double, HistoryLength> frametimes = {};

        /// The amount of time spend rendering the 2D and 3D components of the frame
        std::array<double, HistoryLength> drawTimes = {};

        /// The amount of time spend synchronizing the state between master and clients
        std::array<double, HistoryLength> syncTimes = {};

        /// The lowest time recorded for network communication between master and clients
        std::array<double, HistoryLength> loopTimeMin = {};

        /// The highest time recorded for network communication between master and clients
        std::array<double, HistoryLength> loopTimeMax = {};

        /**
         * \return The frame time (delta time) in seconds
         */
        double dt() const;

        /**
         * \return The average frame time (delta time) in seconds
         */
        double avgDt() const;

        /**
         * \return the minimum frame time (delta time) in the averaging window (seconds)
         */
        double minDt() const;

        /**
         * \return the maximum frame time (delta time) in the averaging window (seconds)
         */
        double maxDt() const;
    };

    struct SGCT_EXPORT Settings {
        /// Stores the configuration option whether the created OpenGL contexts should be
        /// debug contexts or regular ones. This value is only in use between the
        /// constructor and the #initialize function
        bool createDebugContext = false;

        /// Sets the swap interval to be used by the application
        ///   -1 = adaptive sync(Nvidia)
        ///    0 = vertical sync off
        ///    1 = wait for vertical sync
        ///    2..inf = wait for every n-th vertical sync
        int8_t swapInterval = 1;

        /// If this is true, a log message is printed to the console while a client is
        /// waiting for the master to connect or while the master is waiting for one or
        /// more clients
        bool printSyncMessage = true;

        /// Get if capture should use backbuffer data or texture. Backbuffer data includes
        /// masks and warping
        bool captureBackBuffer = false;

        bool useDepthTexture = false;
        bool useNormalTexture = false;
        bool usePositionTexture = false;

        /// The number of seconds that SGCT will wait for the master or clients to connect
        /// before aborting
        float syncTimeout = 60.f;

        struct SS{
            /// The location where the screenshots are being saved
            std::filesystem::path capturePath;

            // The number of capture threads
            int nCaptureThreads = std::max(std::thread::hardware_concurrency() / 2, 1u);

            /// If set to true, the node name is added to screenshots
            bool addNodeName = false;

            /// If set to true, the window name is added to screenshots
            bool addWindowName = true;

            /// The prefix to be used for all screenshots
            std::string prefix;

            /**
             * Information about the screenshot limits. If there is no screenshot limit,
             * this function returns `std::nullopt`. Otherwise the first component is the
             * index of the first screenshot that will be rendered. The second component
             * is the index of the last screenshot that will not be rendered anymore.
             */
            std::optional<std::pair<uint64_t, uint64_t>> limits;
        } capture;
    };

    /**
     * This struct holds all of the callback functions that can be used by the client
     * library to be called during the different times of the frame.
     */
    struct SGCT_EXPORT Callbacks {
        /// This function is called before the window is created (before OpenGL context is
        /// created). At this stage the configuration file has been read and network
        /// is initialized.
        void (*preWindow)() = nullptr;

        /// This function is called once before the starting the render loop and after
        /// creation of the OpenGL context. The window that is passed in this callback is
        /// the shared context between all created windows
        void (*initOpenGL)(GLFWwindow*) = nullptr;

        /// This function is called before the synchronization stage.
        void (*preSync)() = nullptr;

        /// This function is called once per frame after sync but before draw stage.
        void (*postSyncPreDraw)() = nullptr;

        /// This function draws the scene and could be called several times per frame
        /// as it's called once per viewport and once per eye if stereoscopy is used.
        void (*draw)(const RenderData&) = nullptr;

        /// This function is be called after overlays and post effects has been drawn and
        /// can used to render text and HUDs that will not be filtered or antialiased.
        void (*draw2D)(const RenderData&) = nullptr;

        /// This function is called after the draw stage but before the OpenGL buffer
        /// swap.
        void (*postDraw)() = nullptr;

        /// This is called before all SGCT components will be destroyed. The same shared
        /// context is active that was passed in the Callbacks::initOpenGL callback
        void (*cleanup)() = nullptr;

        /// This function is called to encode all shared data that is sent to the
        /// connected nodes in a clustered setup.
        std::vector<std::byte> (*encode)() = nullptr;

        /// This function is called by decode all shared data sent to us from the master
        /// The parameter is the block of data that contains the data to be decoded.
        void (*decode)(const std::vector<std::byte>&) = nullptr;

        /// This function is called when a TCP message is received.
        void (*externalDecode)(const char*, int) = nullptr;

        /// This function is called when the connection status changes.
        void (*externalStatus)(bool) = nullptr;

        /// This function is called when a TCP message is received.
        void (*dataTransferDecode)(void*, int, int, int) = nullptr;

        /// This function is called when the connection status changes.
        void (*dataTransferStatus)(bool, int) = nullptr;

        /// This function is called when data is successfully sent.
        void (*dataTransferAcknowledge)(int, int) = nullptr;

        /// This function sets the keyboard callback (GLFW wrapper) for all windows.
        void (*keyboard)(Key, Modifier, Action, int, Window*) = nullptr;

        /// All windows are connected to this callback.
        void (*character)(unsigned int, int, Window*) = nullptr;

        /// This function sets the mouse button callback (GLFW wrapper) for all windows.
        void (*mouseButton)(MouseButton, Modifier, Action, Window*) = nullptr;

        /// All windows are connected to this callback.
        void (*mousePos)(double, double, Window*) = nullptr;

        /// All windows are connected to this callback.
        void (*mouseScroll)(double, double, Window*) = nullptr;

        /// Drop files to any window. All windows are connected to this callback.
        void (*drop)(const std::vector<std::string_view>&) = nullptr;
    };

    /**
     * Returns the global Engine object that is created through the Engine::create
     * function. This function must only be called after the Engine::create function has
     * been called successfully.
     *
     * \return The global Engine object responsible for this application
     *
     * \throw std::logic_error This error is thrown if this function is called before the
     *        Engine::create function is called or after the Engine::destroy function was
     *        called
     */
    static Engine& instance();

    /**
     * Creates the singleton Engine that is accessible through the Engine::instance
     * function. This function can only be called while no Engine instance exists, which
     * means that either it has to be the first call to this function or the #destroy
     * function was called in between.
     *
     * \param cluster The configuration object for the config::Cluster that contains the
     *        information about how many nodes should exist, how many windows each node
     *        should contain and all other potential objects. The result of this is, in
     *        general, loaded from a configuration file
     * \param callbacks The list of callbacks that should be registered and called during
     *        the frame and their correct times. All callbacks are optional
     * \param arg The parameters that were set by the user from the commandline that can
     *        override some of the behavior from the configuration
     */
    static void create(config::Cluster cluster, Callbacks callbacks,
        const Configuration& arg);

    /**
     * Destroys the singleton Engine instance that was created by Engine::create and that
     * is accessible through the Engine::instance function. If this function is called
     * without a valid singleton existing, it is a no-op.
     */
    static void destroy();

    /**
     * Signals to SGCT that the application should be terminated at the end of the next
     * frame.
     */
    void terminate();

    /**
     * This function starts the SGCT render loop in which the rendering, synchronization,
     * event handling, and everything else happens. Control will only return from this
     * function after the program is terminated for any reason or if a non-recoverable
     * error has occurred.
     */
    void exec();

    /**
     * Returns the Engine::Statistics object that contains all collected information about
     * the frametimes, drawtimes, and other frame-based statistics. The reference returned
     * by this function is valid until the Engine::destroy function is called.
     *
     * \return The Statistics object containing all of the statistics information
     */
    const Statistics& statistics() const;

    /**
     * Returns the distance to the near clipping plane in meters.
     *
     * \return The distance to the near clipping plane in meters
     */
    float nearClipPlane() const;

    /**
     * Returns the distance to the far clipping plane in meters.
     *
     * \return The distance to the far clipping plane in meters
     */
    float farClipPlane() const;

    /**
     * Set the near and far clipping planes. This operation recalculates all frustums for
     * all viewports.
     *
     * \param nearClippingPlane The near clipping plane in meters
     * \param farClippingPlane The far clipping plane in meters
     */
    void setNearAndFarClippingPlanes(float nearClippingPlane, float farClippingPlane);

    /**
     * This functions updates the frustum of all viewports. If a viewport is tracked, this
     * is done on the fly.
     */
    void updateFrustums() const;

    /**
     * Return the Window that currently has the focus. If no SGCT window has focus, a
     * `nullptr` is returned.
     *
     * \return The focus window or `nullptr` if no such window exists
     */
    const Window* focusedWindow() const;

    /**
     * Determines whether the graph displaying the rendering stats is being displayed or
     * not.
     *
     * \param value The new desired state as to whether the statistics are being shown
     */
    void setStatsGraphVisibility(bool value);

    /**
     * Returns the current scaling value used to render the statistics graphs if they are
     * enabled. If the statistics graphs are currently not shown, an error value of `-1`
     * is returned instead.
     *
     * \return The current scaling value for the statistics graph
     */
    float statsGraphScale() const;

    /**
     * Sets the new scaling value for the statistics graph rendering. This value must be
     * in the range [0,1]. If the statistics graph is currently not showing, calling this
     * function will not have any effects.
     *
     * \param scale The new scaling value for the statistics graph rendering
     */
    void setStatsGraphScale(float scale);

    /**
     * Returns the current offset value used to render the statistics graphics away from
     * the center of the screen if they are enabled. If the statistics graphs are
     * currently not shown, an error value of `{-1, -1}` is returned instead.
     *
     * \return The current offset value for the statistics graph
     */
    vec2 statsGraphOffset() const;

    /**
     * Sets the new offet value for the statistics graph rendering. This value determines
     * the placement of the center of the graphs on the screen and is usually in the range
     * of [0,1]. If the statistics graph is currently not showing, calling this function
     * will not have any effects.
     *
     * \param offset The new offset value for the statistics graph rendering
     */
    void setStatsGraphOffset(vec2 offset);

    /**
     * Takes an RGBA screenshot and saves it as a PNG file. If stereo rendering is enabled
     * then two screenshots will be saved per frame, one for each eye. The filename for
     * each image is the window title with an incremental counter appended to it. Each
     * successive call of this function will increment the counter. If it is desired to
     * reset the counter, see #setScreenshotNumber.
     *
     * To record frames for a movie simply call this function every frame you wish to
     * record. The write to disk is multi-threaded.
     *
     * \param windowIds If the vector is empty, screenshots of all windows will be taken,
     *        otherwise only the Window ids that appear in the vector will be used for
     *        screenshots and Window ids that do not appear in the list are ignored
     */
    void takeScreenshot(std::vector<int> windowIds = std::vector<int>());

    /**
     * Resets the screenshot number to 0.
     */
    void resetScreenshotNumber();

    /**
     * Sets the number that the next screenshot will recieve with the next call of
     * #takeScreenshot.
     *
     * \param number The next screenshot number
     */
    void setScreenshotNumber(unsigned int number);

    /**
     * Returns the number the next screenshot will receive upon the next call of
     * #takeScreenshot. This counter can be reset through #setScreenshotNumber.
     *
     * \return The number the next screenshot will receive
     */
    unsigned int screenShotNumber() const;

    /**
     * This function returns the draw function to be used in internal classes that need to
     * repeatedly call this. In general, there is no need for external applications to
     * store the draw function, but they are free to do so.
     *
     * \return The currently bound draw function
     */
    Engine::DrawFunction drawFunction() const;

    Engine::DrawFunction draw2DFunction() const;


    /**
     * Returns a reference to the node that represents this computer.
     *
     * \return A reference to this node
     */
    const Node& thisNode() const;

    /**
     * Returns a list of all windows for the current node. This vector might be empty, and
     * is valid for the lifetime of the program until the Engine::destroy function is
     * called.
     *
     * \return A list of all windows for the current node
     */
    const std::vector<std::unique_ptr<Window>>& windows() const;

    /**
     * Returns a pointer to the default user, i.e. the observer position.
     *
     * \return A pointer to the default user
     */
    static User& defaultUser();

    /**
     * Returns if this Node is the master in a clustered environment. This function will
     * also return `true` if the Node is not part of a clustered environment.
     *
     * \return `true` if this Node is the master in a clustered environment or not part of
     *         a cluster; `false` otherwise
     */
    bool isMaster() const;

    /**
     * Returns the number of the current frame. The frame number is a monotonically
     * increasing number that will never be repeated.
     *
     * \return The current framenumber
     */
    unsigned int currentFrameNumber() const;

    /**
     * Set capture/screenshot path used by SGCT.
     *
     * \param path The path including filename without suffix
     */
    void setCapturePath(std::filesystem::path path);

    /**
     * Set if capture should capture warped from backbuffer instead of texture. Backbuffer
     * data includes masks and warping.
     */
    void setCaptureFromBackBuffer(bool state);

    StatisticsRenderer* statisticsRenderer();

    const Settings& settings() const;

private:
    /**
     * The global singleton instance of this Engine class. This instance is created
     * through the #create function, accessed through the #instance function, and removed
     * through the #destroy function.
     */
    static Engine* _instance;

    /**
     * The internal constructor for this class, which will be called by the #create
     * function. See the #create function for more documentation on the parameters.
     *
     * \param cluster The cluster setup that should be used for this SGCT run
     * \param callbacks The list of callbacks that should be installed
     * \param config Parameters that were set by the user from the commandline
     */
    Engine(config::Cluster cluster, Callbacks callbacks, const Configuration& config);

    /**
     * Engine destructor destructs GLFW and releases resources/memory.
     */
    ~Engine();

    /**
     * Two-phase initialization that will setup all of the required OpenGL state and other
     * states that are necessary to run the Engine instance created by the constructor.
     */
    void initialize();

    /**
     * Creates and initializes all of the windows that are specified for the current Node.
     * This function will call the Callbacks::preWindow callback for each window created
     * before the OpenGL context has been created and initialized.
     *
     * \param majorVersion The major version for OpenGL that is requested
     * \param minorVersion The minor version for OpenGL that is requested
     *
     * \pre \p majorVersion must be bigger than 0
     * \pre \p minorVersion must be bigger than 0
     */
    void initWindows(int majorVersion, int minorVersion);

    /**
     * Locks the rendering thread for synchronization. Locks the clients until data is
     * successfully received.
     */
    void frameLockPreStage();

    /**
     * Locks the rendering thread for synchronization. Locks master until clients are
     * ready to swap buffers.
     */
    void frameLockPostStage();

    /**
     * This function waits for all windows to be created on the whole cluster in order to
     * set the barrier (hardware swap-lock). Under some Nvidia drivers the stability is
     * improved by first join a swapgroup and then set the barrier then all windows in a
     * swapgroup are created.
     */
    void waitForAllWindowsInSwapGroupToOpen();

    /// The function pointer that is called before any windows are created
    void (*_preWindowFn)() = nullptr;

    /// The function pointer that is called after all windows have been created
    void (*_initOpenGLFn)(GLFWwindow*) = nullptr;

    /// Function pointer that is called before the synchronization step of the frame
    void (*_preSyncFn)() = nullptr;

    /// Function pointer that is called after the synchronization but before rendering
    void (*_postSyncPreDrawFn)() = nullptr;

    /// Function pointer that is called for the 3D portion of the rendering
    void (*_drawFn)(const RenderData&) = nullptr;

    /// Function pointer that is called for the 2D portion of the rendering
    void (*_draw2DFn)(const RenderData&) = nullptr;

    /// Function pointer that is called after all rendering has finished
    void (*_postDrawFn)() = nullptr;

    /// Function pointer that is called when the Engine is being destroyed
    void (*_cleanupFn)() = nullptr;

    /// The near clipping plane used in the rendering and set through
    /// #setNearAndFarClippingPlanes
    float _nearClipPlane = 0.1f;

    /// The far clipping plane used in the rendering and set through
    /// #setNearAndFarClippingPlanes
    float _farClipPlane = 100.f;

    /// The container for the per-frame statistics that are being collected
    Statistics _statistics;

    /// Stores the previous frametime so that a delta frametime can be calculated
    double _statsPrevTimestamp = 0.0;

    /// The class that renders the on-screen representation of the Statistics data. If
    /// this pointer is `nullptr` then no rendering is performed
    std::unique_ptr<StatisticsRenderer> _statisticsRenderer;

    /// Whether SGCT should take a screenshot in the next frame
    bool _shouldTakeScreenshot = false;

    /// Whether SGCT should terminate in the next frame
    bool _shouldTerminate = false;

    /// Contains the list of window ids that should have a screenshot taken. If this
    /// vector is empty, all windows will have a screenshot
    std::vector<int> _shouldTakeScreenshotIds;

    Settings _settings;

    std::unique_ptr<std::thread> _thread;

    unsigned int _frameCounter = 0;
    unsigned int _shotCounter = 0;
};

} // namespace sgct

#endif // __SGCT__ENGINE__H__
