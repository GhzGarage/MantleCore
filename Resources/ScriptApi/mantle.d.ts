// Mantle scripting API for JavaScript/TypeScript. GENERATED from the engine's API registry; do not edit.
// Referenced by tsconfig.json so VS Code type-checks Content/**/Scripts/**/*.ts against this file.
export {};

declare global {

interface Signal<T extends any[] = any[]> {
	connect(handler: (...args: T) => void): Connection;
	/** Fire a custom event from Events.get. */
	fire(...args: T): void;
	once(handler: (...args: T) => void): Connection;
	wait(): Promise<T extends [infer Only] ? Only : T>;
}

class Vector3 {
	constructor(x?: number, y?: number, z?: number);
	readonly x: number;
	readonly y: number;
	readonly z: number;
	readonly magnitude: number;
	readonly unit: Vector3;
	add(other: Vector3 | number): Vector3;
	sub(other: Vector3 | number): Vector3;
	mul(other: Vector3 | number): Vector3;
	div(other: Vector3 | number): Vector3;
	dot(other: Vector3): number;
	cross(other: Vector3): Vector3;
	lerp(goal: Vector3, alpha: number): Vector3;
	distance(other: Vector3): number;
	equals(other: Vector3): boolean;
	toString(): string;
	static readonly zero: Vector3;
	static readonly one: Vector3;
	static readonly xAxis: Vector3;
	static readonly yAxis: Vector3;
	static readonly zAxis: Vector3;
}

/** A thing in the world: any actor in the level. */
interface Entity {
	/** Whether a physics entity is held where it is (true) or simulating (false); the place's physics.simulate says how it starts. Set it on the server to drop or pin it. Every other entity is always anchored, and setting it on one errors. */
	anchored: boolean;
	/** How fast the entity spins, in degrees per second about the world axes (X, Y, Z). Setting it (server only) replaces a physics entity's spin; setting it on an Anchored entity, or one with no physics block, errors. Anything that isn't simulating reads (0, 0, 0). */
	angularVelocity: Vector3;
	/** Whether players and objects bump into it. */
	canCollide: boolean;
	/** How many seconds into its clip a SkeletalMesh is now, the same on every screen; nil when it plays no clip, while the clip is still downloading, and always on a dedicated server (it never loads clips). Read-only. Errors when the entity is not a SkeletalMesh. */
	readonly clipTime: number | undefined;
	/** A mesh's tint as "#RRGGBB", multiplied into its materials; "#FFFFFF" is none. Setting it on the server changes it for everyone, players who join later included; on a client it only changes that client's view. Only materials uploaded with the place can be tinted: errors on an entity with none of those, or that is not a mesh. */
	color: string;
	/** False once the entity has been destroyed. */
	readonly exists: boolean;
	/** The light's colour as "#RRGGBB", the same format as Vehicles:SetPaintColor. Replicates like LightEnabled. */
	lightColor: string;
	/** Whether the light shines. Setting it on the server changes it for everyone, players who join later included; on a client it only changes that client's view. Errors if the entity is not a light. */
	lightEnabled: boolean;
	/** A spot light's fully lit cone, in degrees, clamped to 0..90. Errors on other lights. Replicates like LightEnabled. */
	lightInnerCone: number;
	/** How bright the light is, in its own units: a place light keeps the units it was made with, a Lights:Spawn light uses candelas, and a directional light uses lux. Clamped to 0..100000 (0..200 for a directional light). Replicates like LightEnabled. */
	lightIntensity: number;
	/** "Point", "Spot" or "Directional" when the entity is a light, nil for anything else. Read-only. */
	readonly lightKind: string | undefined;
	/** A spot light's outer cone, where its light fades to nothing, in degrees, clamped to 0..90. Errors on other lights. Replicates like LightEnabled. */
	lightOuterCone: number;
	/** How far a point or spot light reaches, in centimetres, clamped to 1..100000. Errors on a directional light. Replicates like LightEnabled. */
	lightRange: number;
	/** A physics entity's mass in kg: the place's physics.mass, else computed from its collision. 0 for an entity with no physics block. Read-only. */
	readonly mass: number;
	/** The actor's label. */
	readonly name: string;
	/** A physics entity's surface: "default", "rubber" (bouncy), "ice" (slippery), "metal", "wood" or "stone"; the place's physics.material says how it starts. Set it on the server, anchored or not, and every player sees the change. nil for an entity with no physics block, and setting it on one errors, as does any other name. */
	physicsMaterial: string | undefined;
	/** World location in centimetres. Setting it on the server replicates to everyone; on a client it only changes that client's view. */
	position: Vector3;
	/** Rotation in degrees as (pitch, yaw, roll). */
	rotation: Vector3;
	/** Scale (1, 1, 1 is normal size). */
	scale: Vector3;
	/** The id of a sound emitter's sound, to pass to Sounds:Play or Sounds:PlayMusic; nil for any other entity. */
	readonly soundId: string | undefined;
	/** Sound emitters only: the emitter's volume, 0 to 2. Setting it from a server script changes it for every player. */
	soundVolume: number;
	/** How see-through a mesh is, from 0 (solid) to 1 (invisible); values outside are clamped. Replicates like Color. Only materials uploaded with the place can be made see-through: errors on an entity with none of those. */
	transparency: number;
	/** How fast the entity moves, in cm/s. Setting it (server only) replaces a physics entity's velocity, and does nothing while it is Anchored; setting it on an entity with no physics block errors. */
	velocity: Vector3;
	/** Whether it is drawn. Hidden entities still collide unless CanCollide is false. */
	visible: boolean;
	/** An attribute on this entity changed. */
	readonly attributeChanged: Signal<[name: string, value: any]>;
	/** A physics entity started touching something (server only): another entity, a player's character, or nil for anything else. Fires once when a contact starts, not while it lasts, and at most a few dozen times a tick across the place. Only physics entities fire it. */
	readonly collided: Signal<[other: Entity | undefined]>;
	/** A player's character stopped touching this entity. */
	readonly touchEnded: Signal<[player: Player]>;
	/** A player's character started touching this entity. */
	readonly touched: Signal<[player: Player]>;
	/** Give the entity a tag (fires World:OnTagged handlers). (server only) */
	addTag(tag: string): void;
	/** Spin a physics entity: an angular impulse in kg*cm^2/s about the world axes, in degrees. Does nothing while it is Anchored. Errors on an entity with no physics block. (server only) */
	applyAngularImpulse(impulse: Vector3): void;
	/** Push a physics entity: impulse is in kg*cm/s along the world axes, so its velocity changes by impulse / Mass. Does nothing while it is Anchored. Errors on an entity with no physics block. (server only) */
	applyImpulse(impulse: Vector3): void;
	/** Make a copy (same look, tags and place). On the server the copy appears for everyone; on a client only for that player. Tip: keep a hidden template and clone it. */
	clone(): Entity;
	/** Remove the entity. On the server it disappears for everyone; on a client only for that player. */
	destroy(): void;
	/** A custom value stored on the entity, or nil. */
	getAttribute(name: string): any;
	/** The material asset id a mesh shows in a slot (0 when left out), or nil when the slot shows the mesh's own material. */
	getMaterial(slot?: number | undefined): string | undefined;
	/** All of the entity's tags. */
	getTags(): (string)[];
	/** Whether the entity has this tag. */
	hasTag(tag: string): boolean;
	/** Call handler with the attribute's value now and every time it changes. */
	observeAttribute(name: string, handler: (value: any) => void): Connection;
	/** Play one of the place's AnimClip assets on a SkeletalMesh, starting now. Leave clipAssetId out (or nil) to restart the clip it plays now, or its own. params: Loop (true by default; false holds the last frame), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). On the server it plays for everyone, players who join later included, in step on every screen; on a client it only changes that client's view. Errors when the entity is not a SkeletalMesh, the id is not an AnimClip in the place's assets, or the clip was authored on another skeleton (on a dedicated server, which never loads clips, that last one shows on each player's screen as the mesh's resting pose instead). */
	playClip(clipAssetId?: string | undefined, params?: { loop?: boolean | undefined; playRate?: number | undefined; startOffset?: number | undefined } | undefined): void;
	/** Effects only: start emitting. A looping effect already playing keeps going; one that does not loop bursts for a second again. On the server it plays for everyone, players who join later included; on a client it only changes that client's view. Errors when the entity is not an Effect. */
	playEffect(): void;
	/** Sound emitters only: play the emitter's sound again from the beginning (looping if the emitter loops). From a server script every player hears it. */
	playSound(): void;
	/** Remove a tag (fires World:OnUntagged handlers). (server only) */
	removeTag(tag: string): void;
	/** Store a custom value on the entity (numbers, strings, booleans, Vector3, tables). Server values replicate. */
	setAttribute(name: string, value: any): void;
	/** Effects only: tint the particles, "#RRGGBB"; the next particles take it. Replicates like PlayEffect. Errors when the entity is not an Effect or the colour is not "#RRGGBB". */
	setEffectColor(color: string): void;
	/** Effects only: how many particles spawn a second, clamped to 0..200; 0 emits nothing while it still plays. Replicates like PlayEffect. Errors when the entity is not an Effect. */
	setEffectRate(rate: number): void;
	/** Show another of the place's Material assets on this mesh: in one material slot (counting from 0), or in every slot when slot is left out. "" puts the place's own material back. Replicates like Color. Errors when the id is not a Material in the place's assets, the slot doesn't exist, or the entity is not a mesh. */
	setMaterial(assetId: string, slot?: number | undefined): void;
	/** Stop a SkeletalMesh's clip: it shows its resting pose until the next PlayClip. Replicates like PlayClip. Errors when the entity is not a SkeletalMesh. */
	stopClip(): void;
	/** Effects only: stop emitting; the particles already out die out. Replicates like PlayEffect. Errors when the entity is not an Effect. */
	stopEffect(): void;
	/** Sound emitters only: stop the emitter's sound, fading it out over fadeOut seconds (0 by default). It stays silent until PlaySound, from a server script for players who join later too. */
	stopSound(fadeOut?: number | undefined): void;
	/** Smoothly move/rotate/scale to the goal. Since script API 1.8 the goal can also fade a light's LightColor or LightIntensity, or a mesh's Color; those errors like setting them would. style: Linear, Sine, Quad, Cubic, Quart, Quint, Exponential, Circular, Back, Bounce, Elastic. direction: In, Out, InOut. Returns a Completed signal. */
	tween(goal: { position?: Vector3 | undefined; rotation?: Vector3 | undefined; scale?: Vector3 | undefined; lightColor?: string | undefined; lightIntensity?: number | undefined; color?: string | undefined }, seconds: number, style?: string | undefined, direction?: string | undefined): Signal;
}

/** A connected player. */
interface Player {
	/** The item id of the avatar (body) the player has on, or nil before their look has arrived. Read-only. (server only) */
	readonly avatar: string | undefined;
	/** The player's body in the world, or nil while respawning. */
	readonly character: Entity | undefined;
	/** Whether the player can crouch or slide (true unless a script changes it); nil while the player has no character. Setting it (server only) to false stands a crouching player up and the crouch key does nothing until it is true again. It lasts for the rest of the session, through respawns. */
	crouchEnabled: boolean | undefined;
	/** How fast the character moves while crouched, in centimetres per second (225 unless a script changes it; it scales with WalkSpeed until you set it). nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns. */
	crouchSpeed: number | undefined;
	/** The character's health, from 0 to MaxHealth; nil while the player has no character. Weapons and TakeDamage lower this same value. Setting it (server only) clamps to 0..MaxHealth, and 0 kills the character with no killer. Setting it on a dead player, or one driving a vehicle, does nothing: use Respawn to bring a player back. */
	health: number | undefined;
	/** Whether the character is crouched right now (by the key, a slide or ForceCrouch). nil while the player has no character. Read-only. */
	readonly isCrouched: boolean | undefined;
	/** Whether the character is limp on the ground right now: from Ragdoll, or a hard hit, a blast or being thrown off a vehicle knocked it down. nil while the player has no character. Read-only. */
	readonly isRagdolled: boolean | undefined;
	/** Whether the character is sprinting right now: holding the sprint key and moving forward fast enough. nil while the player has no character. Read-only. */
	readonly isSprinting: boolean | undefined;
	/** How high the character's jump rises, in centimetres (about 90 under normal gravity unless a script changes it). nil while the player has no character. Setting it (server only) clamps to 0..1000 and lasts for the rest of the session, through respawns; 0 stops ordinary jumps. */
	jumpHeight: number | undefined;
	/** How much stamina a jump costs (0 unless a script changes it, so jumping is free); with a cost, a character with less stamina than that cannot jump. nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session. Only counts while StaminaEnabled is true. */
	jumpStaminaCost: number | undefined;
	/** The most health the character can have (100 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 1..10000 and lasts for the rest of the session: every respawn starts with full health at this maximum. Lowering it below Health lowers Health too; raising it does not heal. */
	maxHealth: number | undefined;
	/** The most stamina the character can have (100 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 1..10000 and lasts for the rest of the session, through respawns. Lowering it below Stamina lowers Stamina too; raising it does not refill. */
	maxStamina: number | undefined;
	/** The player's Epic display name. */
	readonly name: string;
	/** Where the player's character is. */
	readonly position: Vector3 | undefined;
	/** Whether the player can sprint (true unless a script changes it); nil while the player has no character. Setting it (server only) to false stops a sprint at once and the sprint key does nothing until it is true again. It lasts for the rest of the session, through respawns. */
	sprintEnabled: boolean | undefined;
	/** How fast the character sprints, in centimetres per second (700 unless a script changes it; it scales with WalkSpeed until you set it). nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns. */
	sprintSpeed: number | undefined;
	/** The stamina the character has left, from 0 to MaxStamina (it counts in small steps, the same on every machine, and moves only while StaminaEnabled is true); nil while the player has no character. Setting it (server only) clamps to 0..MaxStamina. Every new character starts full. */
	stamina: number | undefined;
	/** How much stamina sprinting uses each second (20 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session. */
	staminaDrain: number | undefined;
	/** Whether the player has a stamina pool (false unless a script turns it on, so places that never use it play as before); nil while the player has no character. With it on, sprinting drains Stamina, Stamina refills after a short rest, a sprint stops at zero and cannot restart until a tenth of MaxStamina has come back, and jumping can cost JumpStaminaCost. Turning it on starts the pool full. Setting it (server only) lasts for the rest of the session, through respawns. */
	staminaEnabled: boolean | undefined;
	/** How much stamina comes back each second once the character has rested (15 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..10000 and lasts for the rest of the session. */
	staminaRegen: number | undefined;
	/** How many seconds after sprinting (or a jump that cost stamina) before stamina starts coming back (1 unless a script changes it); nil while the player has no character. Setting it (server only) clamps to 0..60 and lasts for the rest of the session. */
	staminaRegenDelay: number | undefined;
	/** The player's verified Mantle account id (a plain UUID). */
	readonly userId: string;
	/** How fast the character runs, in centimetres per second (500 unless a script changes it); walking, sprinting and crouching scale with it. nil while the player has no character. Setting it (server only) clamps to 0..1500 and lasts for the rest of the session, through respawns; 0 stops the character moving. */
	walkSpeed: number | undefined;
	/** What the player is wearing: slot name ("hair", "top", "footwear", "acc_hat", ...) to item id. Empty when nothing is worn or their look has not arrived. Read-only; a fresh table on every read. (server only) */
	readonly wearing: Record<string, string>;
	/** The character crouched (the key, a slide or ForceCrouch). Fires on the server and on every client. */
	readonly crouchStarted: Signal<[player: Player]>;
	/** The character stood up from a crouch. Fires on the server and on every client. */
	readonly crouchStopped: Signal<[player: Player]>;
	/** The player's character died, on the server and on every client. killer is the player whose damage finished it (a weapon, or TakeDamage with a source); nil for falling out of the world, Kill, Health set to 0, or their own damage. */
	readonly died: Signal<[player: Player, killer: Player | undefined]>;
	/** The character's health changed: damage, healing, Health set, or a respawn's refill. Fires on the server and on every client. */
	readonly healthChanged: Signal<[player: Player, old: number, new: number]>;
	/** The character was knocked down (Ragdoll, a hard hit, a blast, or being thrown off a vehicle). Fires on the server and on every client. */
	readonly ragdolled: Signal<[player: Player]>;
	/** A knocked-down character stood back up (its time ended or Recover was called). It does not fire if the character died while down: Died does. Fires on the server and on every client. */
	readonly recovered: Signal<[player: Player]>;
	/** The character started sprinting. Fires on the server and on every client. */
	readonly sprintStarted: Signal<[player: Player]>;
	/** The character stopped sprinting: the key let go, stamina ran out, SprintEnabled turned off, or it stopped moving. Fires on the server and on every client. */
	readonly sprintStopped: Signal<[player: Player]>;
	/** The character's Stamina changed: sprinting, resting, a jump's cost or a script setting it. It reports steps of about half a percent of MaxStamina, not every frame. Fires on the server and on every client while StaminaEnabled is true. */
	readonly staminaChanged: Signal<[player: Player, old: number, new: number]>;
	/** Hold the player crouched (true) or standing (false) whatever they press, until ReleaseCrouch. A player held crouched can still walk (sprinting is off while crouched); one held standing cannot crouch or slide. CrouchEnabled false wins over a crouch hold. Returns false when the player has no character on foot. The hold ends with the character: a respawn gives the key back. (server only) */
	forceCrouch(crouched: boolean): boolean;
	/** Raise the character's health by amount, up to MaxHealth. Never brings a dead character back (use Respawn). Returns false when the player has no character on foot, is dead, or is already at full health. (server only) */
	heal(amount: number): boolean;
	/** Remove the player from the session with a message. (server only) */
	kick(reason?: string | undefined): void;
	/** Kill the character outright: Died fires with no killer, and the place's respawn rules take over as for any other death. Returns false when the player has no character on foot or is already dead. (server only) */
	kill(): boolean;
	/** Knock the character down: it goes limp for everyone, the player cannot move or act, and when the time is up (or Recover is called) it stands back up where it lies with a get-up animation. seconds is how long to stay down, 0.1 to 300; leave it out to stay down until Recover (at most five minutes). velocity is an optional shove in centimetres per second; without it the character keeps the speed it had. Ragdolled fires now and Recovered when it gets up. Returns false when the player has no character on foot, is dead, is sitting in a vehicle or is already knocked down. (server only) */
	ragdoll(seconds?: number | undefined, velocity?: Vector3 | undefined): boolean;
	/** Stand a knocked-down character back up now, wherever it lies; Recovered fires when it is up. Safe to call at any time. Returns false when the character was not knocked down. (server only) */
	recover(): boolean;
	/** Give the player the crouch key back after ForceCrouch. Returns false when the player has no character on foot. (server only) */
	releaseCrouch(): boolean;
	/** Give the player a fresh character with full health right now, alive or dead: at position when given, otherwise at the spawn point the place's respawn mode picks. The old character (or a corpse waiting for its respawn) is removed. Returns false when the player is driving a vehicle (eject them first) or cannot be spawned. (server only) */
	respawn(position?: Vector3 | undefined): boolean;
	/** Hurt the character through the same path weapons use, so the game's damage rules apply. source is the player to credit: if this hit kills, Died names them as the killer. Returns false when the player has no character on foot (respawning or driving), is already dead, or the damage was refused. (server only) */
	takeDamage(amount: number, source?: Player | undefined): boolean;
	/** Move the player's character instantly. Always lands exactly at position, even inside solid geometry; returns false only when the player has no character to move right now. (server only) */
	teleport(position: Vector3): boolean;
}

/** Returned by Signal:Connect; call Disconnect to stop listening. */
interface Connection {
	/** False once disconnected. */
	readonly connected: boolean;
	/** Stop calling the handler. */
	disconnect(): void;
}

/** A named network event between server and clients. */
interface Remote {
	/** The remote's name. */
	readonly name: string;
	/** The server fired this remote to this player (client only). */
	readonly onClientEvent: Signal<[...args: any[]]>;
	/** A client fired this remote (server only). */
	readonly onServerEvent: Signal<[player: Player, ...args: any[]]>;
	/** Send arguments to every player's OnClientEvent handlers. (server only) */
	fireAllClients(...args: any[]): void;
	/** Send arguments to one player's OnClientEvent handlers. (server only) */
	fireClient(player: Player, ...args: any[]): void;
	/** Send arguments to the server's OnServerEvent handlers. (client only) */
	fireServer(...args: any[]): void;
}

/** One joint between physics entities, from Constraints:Create or Constraints:FromEntity (server only). */
interface Constraint {
	/** The joint broke: something pulled it harder than its BreakForce. Fires once, on the server; the entities move freely from then on. */
	readonly broken: Signal<[constraint: Constraint]>;
	/** Remove the joint for good: the entities move freely again, and Broken never fires. A joint a script made also goes when every script restarts. (server only) */
	destroy(): void;
}

/** One CrowdSpawner's ambient crowd, from Crowds:Get (server only). */
interface Crowd {
	/** How much of the spawner's crowd walks, from 0 (nobody) to 1 (the full count the place set, the default). 0.5 of a crowd of 40 is 20 people. (server only) */
	density: number;
	/** Whether the crowd walks at all. Starts as the spawner's own enabled setting; false removes every one of its people. (server only) */
	enabled: boolean;
	/** True freezes every one of the crowd's people where they stand, the same moment for everyone; false lets them walk on from there. (server only) */
	paused: boolean;
	/** The whole number every player's crowd is made from, 0 to 2147483647. Starts as the spawner's own seed; another seed is another crowd: as many people, on the same lanes, but other people in other places. (server only) */
	seed: number;
}

/** A character NPCs:Spawn made. The server walks and animates it; every player sees it. */
interface NPC {
	/** False once the NPC has been destroyed (Destroy, Entity:Destroy, or it fell out of the world). (server only) */
	readonly exists: boolean;
	/** The NPC's health, from 0 to MaxHealth. Weapons, TakeDamage and Heal change this same value. Setting it clamps to 0..MaxHealth, and 0 kills the NPC with no killer. Setting it on a dead NPC does nothing: use Respawn to bring it back. (server only) */
	health: number;
	/** The most health the NPC can have (100 unless a script changes it). Setting it clamps to 1..10000; lowering it below Health lowers Health too, and raising it does not heal. Respawn refills health to this maximum. (server only) */
	maxHealth: number;
	/** Where the NPC stands: the middle of its body, in centimetres, as Player.Position reads a character. Read-only: use MoveTo to walk it. (server only) */
	readonly position: Vector3;
	/** Which way the NPC faces, in degrees as (pitch, yaw, roll). It turns to face the way it walks. Read-only. (server only) */
	readonly rotation: Vector3;
	/** How fast the NPC walks, in centimetres per second (150 unless a script changes it; a player runs at 500). Clamped to 0..1500; 0 stops it where it is. The walk clip speeds up and slows down with it. (server only) */
	walkSpeed: number;
	/** The NPC died: its health reached 0. killer is the player whose damage finished it (a weapon, or TakeDamage with a source); nil for Kill, Health set to 0, or damage nobody is credited with. A dead NPC stops, falls limp (or plays the clip a Died handler starts with PlayClip or PlayAssetClip, holding its last frame) and stays where it fell, still counting toward the 50, until Respawn or Destroy. Fires on the server only. */
	readonly died: Signal<[npc: NPC, killer: Player | undefined]>;
	/** A drive DriveTo or DriveAlong started ended: result is "arrived" when the car stopped within ArriveRadius of the last point, "stuck" when it could not get further (it reversed and tried again, then gave up), or "cancelled" (StopDriving, a new drive, ExitVehicle, the NPC dying or being destroyed, or the car going away). Fires on the server only, once per drive. */
	readonly driveFinished: Signal<[result: string]>;
	/** The NPC's health changed: damage, healing, Health or MaxHealth set, or a respawn's refill. Fires on the server only. */
	readonly healthChanged: Signal<[npc: NPC, old: number, new: number]>;
	/** A MoveTo walk ended: success is true when the NPC arrived, false when it could not get there. Fires on the server only, once per walk; a walk replaced by another MoveTo, or ended by Stop or Destroy, never fires. */
	readonly moveCompleted: Signal<[success: boolean]>;
	/** Remove the NPC for everyone. Its walk ends without MoveCompleted, and it no longer counts toward the 50. (server only) */
	destroy(): void;
	/** As DriveTo, through points in order (one to 64 positions): the car drives each leg in turn, passing the points on the way without stopping, and ends at the last, which is where it arrives. ArriveRadius is the last point's radius. (server only) */
	driveAlong(vehicle: Entity, points: (Vector3)[], options?: { maxSpeed?: number | undefined; arriveRadius?: number | undefined } | undefined): void;
	/** Put the NPC in the driver's seat of vehicle and drive it to position: the NPC sits in the car for every player, which drives straight at the point, and DriveFinished fires when the drive ends. The NPC must be alive and within 4.5 m of the car with a clear line to the seat; it is the car's driver, so no player gets the Drive prompt or can take the seat, and the car errors if it already has a driver or the NPC is driving another car (StopDriving or ExitVehicle first). It replaces a drive the NPC is on in the same car (that one finishes as "cancelled"). options: MaxSpeed (centimetres per second, 1400 by default, about 50 km/h; clamped to 100..3000) and ArriveRadius (centimetres, 600 by default; clamped to 100..5000). The NPC drives straight at the point over open ground: there are no roads to follow, and it does not steer round walls (a car that can't get on reverses and tries again, then gives up as "stuck"). Cars and trucks only, not motorcycles or aircraft. Arrived, the NPC stays in the seat with the car braked until ExitVehicle. (server only) */
	driveTo(vehicle: Entity, position: Vector3, options?: { maxSpeed?: number | undefined; arriveRadius?: number | undefined } | undefined): void;
	/** Get the NPC out of the car it is driving, standing beside it, and free the seat. A drive in progress ends as "cancelled". Returns false when the NPC is not in a car, the car is still moving faster than a walk (3 m/s; StopDriving, then try again once it has stopped) or there is nowhere to stand beside it. (server only) */
	exitVehicle(): boolean;
	/** The NPC as an Entity: what a raycast that hits it reports. Tag it or give it attributes like any entity. (server only) */
	getEntity(): Entity;
	/** Raise the NPC's health by amount, up to MaxHealth. Never brings a dead NPC back (use Respawn). Returns false when the NPC is dead or already at full health. (server only) */
	heal(amount: number): boolean;
	/** Kill the NPC outright: Died fires with no killer. Returns false when it is already dead. (server only) */
	kill(): boolean;
	/** Walk to position along the ground, around walls and obstacles, replacing any walk in progress. Returns false at once when position is more than 500 m away or the NPC is dead; otherwise true, and MoveCompleted fires when the walk ends: true when the NPC arrived, false when there is no way there (no walkable ground, a blocked or too long path). The first walk in a place waits for its navigation to build, which can take a few seconds. (server only) */
	moveTo(position: Vector3): boolean;
	/** Play one of the place's AnimClip assets on the NPC, starting now, on every player's screen and in step, players who join later included. params: Loop (false by default: the clip plays once and the NPC goes back to walking and standing on its own; true keeps it playing until StopClip or the next PlayClip), PlayRate (clip seconds per second, 1 by default, clamped to 0..10) and StartOffset (seconds into the clip to start at, 0 by default, clamped to 0..3600). The clip must be authored on the skeleton of the NPC's body (the avatars' MetaHuman body). Errors when the id is not an AnimClip in the place's assets or, where the server draws the NPC, the clip is on another skeleton; a dedicated server never loads clips, so there that last one shows on each player's screen as the NPC walking and standing on its own instead. (server only) */
	playAssetClip(clipAssetId: string, params?: { loop?: boolean | undefined; playRate?: number | undefined; startOffset?: number | undefined } | undefined): void;
	/** Play one of the NPC clips on every player's screen: "walk", "idle", "turn_left" or "turn_right", or (script API 1.23) one of the place's AnimClip asset ids at normal speed (PlayAssetClip also sets its speed and start). It plays once and the NPC goes back to walking and standing on its own, or with loop true keeps playing until StopClip or the next PlayClip. Players who join later see it too. (server only) */
	playClip(clip: string, loop?: boolean | undefined): void;
	/** Bring the NPC back with full health, dead or alive: standing at position (its feet) when given, otherwise where NPCs:Spawn first put it. Its clip stops, and a walk in progress ends without MoveCompleted. Returns false when it could not be put there. (server only) */
	respawn(position?: Vector3 | undefined): boolean;
	/** Stop walking where the NPC is. The walk it abandons never fires MoveCompleted. (server only) */
	stop(): void;
	/** End the clip PlayClip started: the NPC walks and stands on its own again. (server only) */
	stopClip(): void;
	/** End the drive the NPC is on: the car brakes to a stop with the NPC still in the seat, and DriveFinished fires with "cancelled". Does nothing when the NPC is not driving. (server only) */
	stopDriving(): void;
	/** Hurt the NPC through the same path weapons use. source is the player to credit: if this hit kills, Died names them as the killer. Returns false when the NPC is already dead or the damage was refused. (server only) */
	takeDamage(amount: number, source?: Player | undefined): boolean;
}

/** A named store of whole-number values for this place, read back sorted: a leaderboard. Get one with DataStore:GetOrderedDataStore (server only). Every method returns a Signal that fires once with a result table {ok, error?, ...}: call :Wait() on it straight away, or :Once(handler). */
interface OrderedDataStore {
	/** Read key's value: the result's value, nil when the store has no such key. (server only) */
	getAsync(key: string): Signal;
	/** One page of the store in value order, highest first (lowest first when ascending): the result's entries is a list of {key, value}, up to pageSize (default 50, at most 100), ties in key order. Pass the result's nextCursor back, with the same ascending, for the next page; it is nil on the last one. (server only) */
	getSortedAsync(ascending: boolean, pageSize?: number | undefined, cursor?: string | undefined): Signal;
	/** Add delta (default 1; may be negative) to key's value, a new key starting from 0. One step on the backend, so no server's increment is lost to another's. The result's value is the new total. (server only) */
	incrementAsync(key: string, delta?: number | undefined): Signal;
	/** Delete key from the store. Removing a key the store doesn't have succeeds. (server only) */
	removeAsync(key: string): Signal;
	/** Store a whole number (up to 2^53 - 1 either way) under key. The result's value is what was stored. (server only) */
	setAsync(key: string, value: number): Signal;
}

/** One sound Sounds:Play started. A server script's handle reaches every player who hears the sound. */
interface SoundHandle {
	/** Whether the sound is still playing (or still downloading, to play when it arrives). False once it is stopped, or a one-shot has finished. A dedicated server never downloads sounds, so there a one-shot counts as playing for up to a minute unless it is stopped. */
	isPlaying(): boolean;
	/** Change the sound's volume, 0 to 2. From a server script, players hear the change within a tenth of a second. */
	setVolume(volume: number): void;
	/** Stop the sound, fading it out over fadeOut seconds (0, the default, stops it at once). Stopping a sound that has already finished does nothing. */
	stop(fadeOut?: number | undefined): void;
}

/** A UI document from the place's UI/ folder, open on the local player's screen (UI:OpenDocument). It belongs to the script that opened it and closes when that script stops. */
interface UiDocument {
	/** True until the document closes: your Close, Back on a modal document, a budget it went over, or its script stopping. (client only) */
	readonly isOpen: boolean;
	/** The document closed by itself: reason is "back" (the player pressed Back on a modal document), "load failed" (its markup or styles could not load), or the budget it went over ("element budget" or "layout time"). Never fires for your own Close, or when your script stops or the place unloads. */
	readonly closed: Signal<[reason: string]>;
	/** The document called emit(name, ...) from a data-event-* attribute; args holds the values it passed. Example: shop.Event:Connect(function(name, args) if name == "buy" then buy(args[1]) end end) */
	readonly event: Signal<[name: string, args: (any)[]]>;
	/** Close the document. Closed does not fire for your own Close, and closing twice is fine. Example: shop:Close() (client only) */
	close(): void;
	/** Set a value the document shows through {{key}}, data-for, data-if and data-attr-*: a number, string, boolean, Vector3 (read as .x, .y and .z), or a table of them. An entity, a player or any other object reference is refused: send its fields instead. The key is a name of letters, digits and _ (up to 64). After the document closes it still checks the key and value, then does nothing. Example: shop:SetData("coins", 120) (client only) */
	setData(key: string, value: any): void;
	/** Switch how the document takes input: "modal", "overlay" or "hud" (see UI:OpenDocument). Does nothing once it has closed. Example: shop:SetMode("hud") (client only) */
	setMode(mode: string): void;
}

/** One node in a script's retained UI tree, mounted into the local player's HUD. */
interface UiNode {
	/** This node's id, stable for its lifetime. */
	readonly id: number;
	/** This node's current text, after any SetLocalizedText resolution. */
	readonly text: string;
	/** This node was activated: a mouse click, Enter, Space, or a focused gamepad Accept on a button, toggle or prompt. Only reaches handlers connected by the script that created this node -- nothing else ever holds its handle. */
	readonly activated: Signal<[id: number, action: string]>;
	/** Remove this node and every descendant. (client only) */
	destroy(): void;
	/** Name the action this node fires on Activated when clicked, tapped or confirmed. (client only) */
	setAction(action: string): boolean;
	/** Attach caption text shown underneath this node when the player has captions on. (client only) */
	setCaption(caption: string): boolean;
	/** Enable or disable this node (a disabled button/toggle/prompt cannot be activated). (client only) */
	setEnabled(enabled: boolean): boolean;
	/** Set an image node's asset (an "ast_" id from the place's asset library). (client only) */
	setImage(assetId: string): boolean;
	/** Look up key in the place's localisation table for the player's locale (falling back through the language, then the place's default locale, then fallback or key itself) and set it as this node's text. (client only) */
	setLocalizedText(key: string, fallback?: string | undefined): boolean;
	/** Reparent this node (nil detaches it back to the root). Refuses a cycle or a node from another script. (client only) */
	setParent(parent?: UiNode | undefined): boolean;
	/** Describe this node for screen readers and controller focus order: "none", "heading", "label", "button", "checkbox", "image", "list-container", "list-item", "dialog" or "status". (client only) */
	setSemantics(role: string, label?: string | undefined, hint?: string | undefined, focusOrder?: number | undefined): boolean;
	/** Replace this node's style: width, height, columns, font-size; grow (0-100, share of the parent's spare height), gap (0-256, space between children), padding (0-256, space inside the node), opacity (0-1), align ("start", "center", "end" or "stretch", within the parent). Returns false for an unknown key or an out-of-range value; a non-finite number is an error. (client only) */
	setStyle(style?: Record<string, boolean | number | string> | undefined): boolean;
	/** Set this node's literal text. (client only) */
	setText(text: string): boolean;
	/** Show or hide this node (and its subtree). (client only) */
	setVisible(visible: boolean): boolean;
}

/** Spawn the place's own meshes at runtime (server only). Change any mesh's look through its Entity. */
interface AssetsService {
	/** Spawn one of the place's StaticMesh assets (any mesh its asset list names, placed or not) that every player sees, and return it as an Entity. props sets how it starts: Rotation, Scale (a Vector3, or one number for all three), Material (one of the place's Material assets, for every slot), Color, Transparency, Collision ("Blocking" by default, "Overlap" or "None"), CastShadow (true by default) and Anchored (spawned meshes stay where scripts put them; false is refused). A mesh uploaded with the place may take a moment to appear on a player's screen while it downloads. Spawned meshes count towards the same limit as Entity:Clone; Destroy one to make room. (server only) */
	spawn(assetId: string, position: Vector3, props?: { rotation?: Vector3 | undefined; scale?: Vector3 | undefined; material?: string | undefined; color?: string | undefined; transparency?: number | undefined; collision?: string | undefined; castShadow?: boolean | undefined; anchored?: boolean | undefined } | undefined): Entity;
}
const Assets: AssetsService;

/** The local player's avatar: open the avatar screen and play their equipped emotes. On the server, read a player's look with Player.Avatar and Player.Wearing. */
interface AvatarService {
	/** Open Mantle's avatar screen, where the player changes their own look. Returns false when there is no local player to open it for. A script cannot change the look itself. (client only) */
	openCustomizer(): boolean;
	/** Play one of the local player's equipped emotes, by its item id (an emote on their emote wheel). Returns false, and plays nothing, when itemId is not one of their equipped emotes, when they have no character, or when called again too soon. true means the request went to the server, which checks it again and can still refuse it (the player is moving, in the air, seated or dead). (client only) */
	playEmote(itemId: string): boolean;
}
const Avatar: AvatarService;

/** Give players the built-in Tools and weapons, take them away, and equip them (server only). */
interface BackpackService {
	/** Empty the player's Backpack, leaving only fists. Returns how many Tools were removed. (server only) */
	clear(player: Player): number;
	/** Put the Tool in this Backpack slot in the player's hands. Slots count from 0, so the hotbar key 1 is slot 0. Returns false when the slot is empty or the player is dead. (server only) */
	equipSlot(player: Player, slot: number): boolean;
	/** The ids GrantBuiltIn accepts in this build: "grenade", "healthpack" and "spawnpad". "fists" is never listed. No weapon is built in (editor test sessions also list their test guns). (server only) */
	getBuiltInTools(): (string)[];
	/** The ids of the Tools in the player's Backpack, in slot order. Fists, which every player always has, is never listed. Empty while the player has no character (respawning). (server only) */
	getTools(player: Player): (string)[];
	/** Give the player a weapon this place stores: item is the library item id Storage:Get returns for one of the place's storage names. Returns the new Tool's stable id. Errors for anything else -- an asset id, a built-in tool's name, an object path, an item the place does not store, or a vehicle. Also errors if the stored weapon has not finished preparing or the player cannot receive it. (server only) */
	grant(player: Player, item: string): string;
	/** Give the player one of the built-in Tools: "grenade", "healthpack" or "spawnpad". Returns the new Tool's id, or nil when the player has no character right now, already has that Tool, or has a full Backpack. A player holding nothing equips it at once. "fists" is accepted and does nothing: every player always has fists. No weapon is built in: give one from the place's storage with Grant. (Editor test sessions also take the test guns GetBuiltInTools lists there.) (server only) */
	grantBuiltIn(player: Player, tool: string): string | undefined;
	/** Take the Tool with this id out of the player's Backpack (unequipping it first). Returns false when the player does not have it. Fists can never be removed. (server only) */
	remove(player: Player, toolId: string): boolean;
	/** Paint one of the player's weapons: color is "#RRGGBB", the same format as Vehicles:SetPaintColor. Every player sees the new colour. Returns false when the player does not have that Tool or it is not a weapon. (server only) */
	setWeaponColor(player: Player, toolId: string, color: string): boolean;
}
const Backpack: BackpackService;

/** The local player's view (client only): first person, a fixed camera (cut to or tweened into), a camera following an entity, the field of view and a shake. All of it goes back to Mantle's own camera when the script stops or the player respawns. */
interface CameraService {
	/** How wide the view is, in degrees, clamped to 30 to 120, or nil for Mantle's own (90). Aiming down sights still zooms in from it. (client only) */
	fieldOfView: number | undefined;
	/** "Default" (Mantle's own camera, which the player can switch between first and third person), "FirstPerson" (held in first person: the player cannot switch out), "Fixed" (set by SetFixed or TweenFixed) or "Follow" (set by Follow). Set it to "Default" or "FirstPerson"; setting "Default" also ends a fixed or following camera. First person applies while on foot. (client only) */
	mode: string;
	/** Keep the camera at offset from entity, looking at it, wherever it goes (Mode becomes "Follow"). offset is in world axes, so Vector3.new(0, -600, 300) keeps the camera 6 m along -Y and 3 m up; with relative true it turns with the entity's heading instead, so Vector3.new(-600, 0, 250) stays behind it. The player still moves their character. When the entity is destroyed the camera goes back to Mantle's; set Mode to "Default" or call Reset to give it back sooner. (client only) */
	follow(entity: Entity, offset: Vector3, relative?: boolean | undefined): void;
	/** Give the camera back to Mantle: Mode "Default", Mantle's field of view, no shake. (client only) */
	reset(): void;
	/** Hold the camera still at position, looking at lookAt (Mode becomes "Fixed"). The player still moves their character. Set Mode to "Default" or call Reset to give the camera back. (client only) */
	setFixed(position: Vector3, lookAt: Vector3): void;
	/** Shake the view, fading out over duration seconds. intensity is clamped to 0 to 1 and duration to 0 to 3; a new shake replaces one still running, and 0 stops it. Does nothing when the player turned on Reduced motion. (client only) */
	shake(intensity: number, duration: number): void;
	/** Move the camera smoothly from where the view is now to position, looking at lookAt, over seconds (0 to 60), then hold it there like SetFixed. Mode is "Fixed" from the start. style and direction ease it as they do Entity:Tween (Linear and Out unless given). Returns a signal that fires once when the camera arrives; it never fires if SetFixed, TweenFixed, Follow, Mode or Reset takes the camera first. (client only) */
	tweenFixed(position: Vector3, lookAt: Vector3, seconds: number, style?: string | undefined, direction?: string | undefined): Signal;
}
const Camera: CameraService;

/** React to what players say in chat, and post lines from the place. */
interface ChatService {
	/** A player said something in chat. text is what everyone saw: already filtered, so masked words stay masked. On the server it fires for every player's line, a second or so after it was sent; on a client, for each player line this player's chat box shows (so never for someone they blocked). Lines from Chat:SendSystemMessage never fire it. */
	readonly messagePosted: Signal<[player: Player, text: string]>;
	/** Post a line from the place into chat, drawn apart from what players say: to everyone, or only to player when you pass one. It goes through the same chat filter as player text, so a line with words that aren't allowed is masked or never shown. At most 500 characters, and at most 10 lines a minute per server. Returns false when it cannot be sent right now: over that limit, the player has no verified account yet, or this server has no session chat (a local test run). (server only) */
	sendSystemMessage(text: string, player?: Player | undefined): boolean;
}
const Chat: ChatService;

/** Join physics entities with welds, hinges and ropes while the place runs (server only). */
interface ConstraintsService {
	/** Join two physics entities, or one to the world, and return the joint. Type is "weld" (holds them as they are), "hinge" (EntityA turns about Axis, and only that) or "rope" (the anchors stay within Length of each other). EntityA is the physics entity the joint holds and EntityB another; leave EntityB out to join EntityA to the world. AnchorA is where on EntityA the joint is, in its own space ((0, 0, 0), its middle, by default); AnchorB where on EntityB (a point in the world without it), by default wherever AnchorA is now. A hinge's Axis is a direction in EntityA's own space ((0, 0, 1) by default) and Limits is {min, max}, the degrees it may turn from where it is now, min at most 0 and max at least 0. A rope's Length is in cm, by default how far apart the anchors are now. Pull the joint harder than BreakForce newtons and it breaks for good (Broken fires). The two entities stop colliding with each other. Players see the entities move; nothing of the joint itself is sent. A place holds at most 200 joints, its own included. (server only) */
	create(params: { type: string; entityA: Entity; entityB?: Entity | undefined; anchorA?: Vector3 | undefined; anchorB?: Vector3 | undefined; axis?: Vector3 | undefined; limits?: (number)[] | undefined; length?: number | undefined; breakForce?: number | undefined }): Constraint;
	/** The joint a place's Constraint entity made, as a Constraint, so a script can hear it break or remove it; nil for any other entity. (server only) */
	fromEntity(entity: Entity): Constraint | undefined;
}
const Constraints: ConstraintsService;

/** Control the ambient crowds a place's CrowdSpawners walk: how many, on or off, moving or frozen (server only). Hear where their people go (client only). */
interface CrowdsService {
	/** One of a CrowdSpawner's people arrived at one of its points of interest and stops there a while (client only). npc says which of the crowd's people it was: the same number is the same person on every player's screen, and they all see the arrival at about the same moment. Play a greeting, open a shop's door, count visitors. Arrivals during a clock re-sync right after joining are not reported. */
	readonly pointReached: Signal<[spawner: Entity, point: Entity, npc: number]>;
	/** The crowd of a CrowdSpawner the place placed. Every player sees what you set on it. Errors if the entity is not a CrowdSpawner (a clone of one has no crowd) or no longer exists. (server only) */
	get(spawner: Entity): Crowd;
}
const Crowds: CrowdsService;

/** Durable saved data for this place (server only). */
interface DataStoreService {
	/** This place's saved value for key, from the local cache (nil if never set, removed, or not loaded yet). (server only) */
	get(key: string): any;
	/** This player's saved value for key, from the local cache. (server only) */
	getForPlayer(player: Player, key: string): any;
	/** The ordered store with this name: whole-number values for this place, read back sorted, for leaderboards. A place can fill 32 stores. (server only) */
	getOrderedDataStore(name: string): OrderedDataStore;
	/** One page (up to 100) of this place's saved keys that start with prefix, in key order, from the local cache like Get. Pass nextCursor back for the next page; it is nil on the last one. (server only) */
	listKeys(prefix?: string | undefined, cursor?: string | undefined): { keys: (string)[]; nextCursor?: string | undefined };
	/** One page of this player's saved keys, like ListKeys. (server only) */
	listKeysForPlayer(player: Player, prefix?: string | undefined, cursor?: string | undefined): { keys: (string)[]; nextCursor?: string | undefined };
	/** Delete key from this place's saved data. Get reads nil at once; removed from the backend asynchronously, like Set. (server only) */
	removeKey(key: string): void;
	/** Delete key from this player's saved data, like RemoveKey. (server only) */
	removeKeyForPlayer(player: Player, key: string): void;
	/** Save key for this place: a boolean, number, string, or table of those (nested up to 4 deep, 64 KB encoded). Applies to the local cache immediately; saved to the backend asynchronously. (server only) */
	set(key: string, value: any): void;
	/** Save key scoped to this player, with the same values as Set. Applies to the local cache immediately; saved to the backend asynchronously. (server only) */
	setForPlayer(player: Player, key: string, value: any): void;
}
const DataStore: DataStoreService;

/** Show Mantle's built-in particle effects anywhere in the world: smoke, fire, sparks, dust and glow. */
interface EffectsService {
	/** Show a built-in effect at position for a moment: archetype is "Smoke", "Fire", "Sparks", "Dust" or "Glow" (any case). It emits for params.Duration seconds (1 by default, 0 to 10) and is gone once its last particles are. params tune its look, each the archetype's own by default: Color ("#RRGGBB"), Size (particle size, 1 to 1000 cm), Rate (particles a second, 0 to 200), Speed (0 to 5000 cm/s), Lifetime (how long each particle lives, 0.05 to 10 seconds); numbers out of range are clamped. Particles leave along +Z, or along Rotation's up (a Vector3 of pitch, yaw and roll). From a client script only this player sees it; from a server script every player does, but not one who joins later. A dedicated server shows nothing. At most 10 plays a frame per script, and a server script at most 10 a second (bursts of 20); past either the script stops. */
	play(archetype: string, position: Vector3, params?: { color?: string | undefined; size?: number | undefined; rate?: number | undefined; speed?: number | undefined; lifetime?: number | undefined; duration?: number | undefined; rotation?: Vector3 | undefined } | undefined): void;
}
const Effects: EffectsService;

/** The time of day, the weather, the sky, the fog and the screen's colour, the same for everyone on the server. Everyone can read them; server scripts can change them, and every player sees the change. */
interface EnvironmentService {
	/** How strongly bright things glow, from 0 (no glow) to 4, or nil for the game's own. Set it on the server. */
	bloom: number | undefined;
	/** The hour, as TimeOfDay. Set it (server) to jump the clock: 21 is night. Any number works and wraps around 24. The clock keeps running from there at TimeScale. */
	clockTime: number;
	/** How much of the sky is cloudy right now, from 0 to 1. */
	readonly cloudCover: number;
	/** A colour-grading look for the whole picture: "Warm", "Cool", "Noir", "Sepia", "Vivid" or "Faded", or nil for none. Set it on the server; ColorGradingIntensity says how strongly it shows. */
	colorGrading: string | undefined;
	/** How strongly ColorGrading shows: 1 is the full look, 0 is none of it, from 0 to 1. Set it on the server. */
	colorGradingIntensity: number;
	/** Tint the whole picture: "#RRGGBB", where "#FFFFFF" is unchanged. Set it on the server; nil sets it back to "#FFFFFF". */
	colorTint: string;
	/** The picture's contrast: 1 is unchanged, from 0 to 2. Set it on the server. */
	contrast: number;
	/** How long one day lasts, in real minutes: 30. */
	readonly dayLength: number;
	/** Brighten (above 0) or darken (below 0) the whole picture, in stops, from -4 to 4. Set it on the server. */
	exposureBias: number;
	/** The fog's colour as "#RRGGBB", or nil for the sky's own. Set it on the server. */
	fogColor: string | undefined;
	/** How thick the fog is, times the weather's own: 1 is unchanged, 0 clears it, up to 4. Set it on the server. */
	fogDensity: number;
	/** True while the sun is below the horizon. */
	readonly isNight: boolean;
	/** How hard it is raining right now, from 0 (dry) to 1 (a storm). */
	readonly rainIntensity: number;
	/** How colourful the picture is: 1 is unchanged, 0 is black and white, up to 2. Set it on the server. */
	saturation: number;
	/** true shows the stars (even by day), false hides them, nil lets them come out at night on their own. Set it on the server. */
	starsVisible: boolean | undefined;
	/** How bright the sun (and by night the moon) is, times its own: 1 is unchanged, 0 to 4. Set it on the server. */
	sunIntensityScale: number;
	/** The hour, from 0 up to 24: 6 is sunrise, 12 is noon, 18 is sunset. Every server starts at 9. */
	readonly timeOfDay: number;
	/** How fast the clock runs: 1 is the normal 30-minute day, 0 stops it, 2 is twice as fast. Set it (server) from 0 to 60; the hour carries on from where it is. */
	timeScale: number;
	/** How dark the picture's corners are, from 0 to 1, or nil for the game's own. Set it on the server. */
	vignette: number | undefined;
	/** The weather, or the weather it is changing to: Clear, PartlyCloudy, Cloudy, Overcast, Foggy, LightRain, Rain, Storm, LightSnow, Snow, Blizzard, DustCalm or DustStorm. */
	readonly weather: string;
	/** Lightning flashed (client only), at the same moment for everyone. The default sky already shows the bolt and plays its thunder. */
	readonly lightningStrike: Signal<[]>;
	/** A server script changed an environment property: the argument names it ("ClockTime", "TimeScale", "Weather", "CloudCover", "FogDensity", "FogColor", "StarsVisible", "SunIntensityScale", "ExposureBias", "Saturation", "Contrast", "Vignette", "ColorTint", "Bloom", "ColorGrading" or "ColorGradingIntensity"). Fires everywhere, a player who joins later included. The clock running on by itself is not a change. */
	readonly propertyChanged: Signal<[property: string]>;
	/** The weather started changing. The argument is the new weather, as in Environment.Weather. */
	readonly weatherChanged: Signal<[weather: string]>;
	/** Take away this player's own look from SetLocalLook (client only): the picture follows the server's values again. (client only) */
	clearLocalLook(): void;
	/** Stop the clock where it is: the same as setting TimeScale to 0. Set TimeScale to 1 to start it again. (server only) */
	freezeClock(): void;
	/** Let the weather change on its own again after SetWeather, blending back over blendSeconds (0 to 600, 90 when left out). (server only) */
	resumeNaturalWeather(blendSeconds?: number | undefined): void;
	/** Cover this much of the sky with cloud, from 0 to 1, whatever the weather; nil gives the weather its own clouds back. The sun dims and the stars fade under heavy cover. (server only) */
	setCloudCover(cover?: number | undefined): void;
	/** Change how the picture looks for this player only (client only). look holds any of ExposureBias (-4 to 4), Saturation (0 to 2), Contrast (0 to 2), Vignette (0 to 1), ColorTint ("#RRGGBB"), Bloom (0 to 4), ColorGrading ("Warm", "Cool", "Noir", "Sepia", "Vivid" or "Faded") and ColorGradingIntensity (0 to 1), taken as the properties of the same names take them. What look sets wins over the server's values; what it leaves out follows them. Each call replaces the last look whole, and the look goes when this script stops. Time and weather stay the same for everyone. From the server, send one player a look with a Remote's FireClient and call this in its OnClientEvent. (client only) */
	setLocalLook(look: { exposureBias?: number | undefined; saturation?: number | undefined; contrast?: number | undefined; vignette?: number | undefined; colorTint?: string | undefined; bloom?: number | undefined; colorGrading?: string | undefined; colorGradingIntensity?: number | undefined }): void;
	/** Hold the weather at Clear, PartlyCloudy, Cloudy, Overcast, Foggy, LightRain, Rain, Storm, LightSnow, Snow, Blizzard, DustCalm or DustStorm until ResumeNaturalWeather, changing to it over blendSeconds (0 to 600, 90 when left out). A Storm brings lightning. The snow and dust weathers (LightSnow, Snow, Blizzard, DustCalm, DustStorm) never come on their own. (server only) */
	setWeather(weather: string, blendSeconds?: number | undefined): void;
}
const Environment: EnvironmentService;

/** Named events that let scripts on the same side talk to each other, in any language. */
interface EventsService {
	/** The event with this name; connect to it or Fire it. Doesn't cross the network (use Net:Remote for that). */
	get(name: string): Signal;
}
const Events: EventsService;

/** The local player's keys, mouse buttons and gamepad buttons, and their sticks, triggers and mouse wheel as axes (client only). Mantle's own controls (the menu, chat, voice, movement, the Backpack and Tool keys) can never be bound or read. */
interface InputService {
	/** Call callback when the player presses one of keys (phase "Began") and when they let go (phase "Ended"); key is the name of the key that did it. keys lists 1 to 4 names such as {"Q", "ButtonY"}: letters, "0" to "9", "F1" to "F10", "Tab", "MouseLeft", "MouseRight", "MouseMiddle", "MouseButton4", "MouseButton5", "ButtonA", "ButtonB", "ButtonX", "ButtonY", "ButtonL1" and "ButtonR1". A key Mantle uses for its own controls is an error; GetBindableKeys lists the free ones. Binding a name again replaces that action. The key still does anything Mantle does with it (F and ButtonY also enter and leave cars). Nothing is heard while Mantle's menu or chat is open. (client only) */
	bindAction(name: string, keys: (string)[], callback: (phase: string, key: string) => void): void;
	/** Call callback when one of axes moves, at most once a frame per axis; axis is the name of the one that moved. axes lists 1 to 4 of "LeftStickX", "LeftStickY", "RightStickX", "RightStickY" (-1 to 1, right and up positive, 0 inside the stick's dead zone), "LeftTrigger", "RightTrigger" (0 to 1) and "MouseWheel" (the notches scrolled that frame, up positive; heard only on frames it moves). A stick or trigger whose movement is one of Mantle's own controls is an error; GetBindableAxes lists the free ones. A trigger Mantle uses as a button (fire, aim) can still be read here: the button keeps working, and no script can bind it as a key. Binding a name again replaces that binding; axis bindings and BindAction's actions have separate names. The control still does anything Mantle does with it (the mouse wheel also steps Mantle's camera in and out). Everything reads 0 while Mantle's menu or chat is open. (client only) */
	bindAxis(name: string, axes: (string)[], callback: (value: number, axis: string) => void): void;
	/** Where axis is right now, by the names BindAxis takes, as its callbacks last heard it: "MouseWheel" reads the notches of the last frame. 0 while Mantle's menu or chat is open. An axis that is part of Mantle's own controls is an error. (client only) */
	getAxis(axis: string): number;
	/** Every axis name this place can bind: the names BindAxis takes, minus the sticks and triggers whose movement Mantle uses for its own controls. A trigger Mantle uses only as a button (fire, aim) is listed. (client only) */
	getBindableAxes(): (string)[];
	/** Every key name this place can bind: the names BindAction takes, minus Mantle's own controls. (client only) */
	getBindableKeys(): (string)[];
	/** Whether the player is holding key down right now, by the same names BindAction takes. Always false while Mantle's menu or chat is open. A key Mantle uses for its own controls is an error. (client only) */
	isKeyDown(key: string): boolean;
	/** Remove the action this script bound under name. Returns false when it has none by that name. A script's actions also go away by themselves when it stops. (client only) */
	unbindAction(name: string): boolean;
	/** Remove the axis binding this script made under name. Returns false when it has none by that name. A script's axis bindings also go away by themselves when it stops. (client only) */
	unbindAxis(name: string): boolean;
}
const Input: InputService;

/** Let players walk up to an object and act on it with a key press, and hear when they do. A place can mark an object interactable itself; here a script makes one, changes what players can do to it, and reacts. The server decides everything: change things and listen to Interacted on the server. The client events are for the local player and only for effects such as a sound or a glow. */
interface InteractionService {
	/** The local player let go of a hold or mash, or its progress drained away, or the part stopped being usable (client only; for effects). A hold or mash that finishes fires Interacted on the server instead. */
	readonly cancelled: Signal<[entity: Entity, part: string, action: string]>;
	/** A player used one of the entity's options (server only). part is the id of the part they used ("" for the object as a whole) and action is "Primary" or "Secondary". A hold or mash fires it once, when it is finished. This is the event that counts: it is checked on the server, so a player out of range, behind a wall or on cooldown never gets here. Connect to it, check which entity it is, and do what the use should do. */
	readonly interacted: Signal<[entity: Entity, player: Player, part: string, action: string]>;
	/** How far the local player's hold or mash has got, 0 to 1, as it changes (client only; for effects such as a rising sound). */
	readonly progress: Signal<[entity: Entity, part: string, action: string, progress: number]>;
	/** The local player's aim came onto a part of an interactable entity and its prompt appeared (client only; for effects, not for changing the game). */
	readonly selected: Signal<[entity: Entity, part: string]>;
	/** The local player began holding or mashing an option (client only; for effects). */
	readonly started: Signal<[entity: Entity, part: string, action: string]>;
	/** The local player's aim left a part, or it stopped being usable (client only; for effects). */
	readonly unselected: Signal<[entity: Entity, part: string]>;
	/** Make the entity not interactable any more, for everyone: no prompt, no key. SetOptions makes it interactable again with the options you give. (server only) */
	remove(entity: Entity): void;
	/** Change how the entity's prompt looks, for everyone. Only the fields you give change. Style is "Default" (the key and text of each option, stacked), "Balanced" (a panel with the Title on top and the options under it) or "Informative" (like Balanced, with the Description beside the options and an accent line); Balanced and Informative need a Title and show as Default without one. Title is up to 80 characters, Description up to 200, and AccentColor is "#RRGGBB". (server only) */
	setDisplay(entity: Entity, display: { style?: string | undefined; title?: string | undefined; description?: string | undefined; accentColor?: string | undefined }): void;
	/** Turn one option of the entity off or back on, for everyone. action is "Primary" or "Secondary", the input the option uses. Turning an option on also ends its cooldown early. An option with AfterUse "OneShot" that was used stays used. Pass a part's id for an object with several parts; leave it out for the object as a whole. (server only) */
	setEnabled(entity: Entity, action: string, enabled: boolean, part?: string): void;
	/** Make the entity interactable and set what players can do to it, for everyone. options is a list of at most two tables, one for each input: Key ("Primary" by default, or "Secondary"), Text (what the prompt says beside the key, such as "Open door"; leave it out to show the key alone), HoldSeconds (hold the key this long; 0 is a tap), PressCount (mash the key this many times instead; wins over HoldSeconds), PressDrainPerSecond (how fast a mash's progress drains, 0 to 10), AfterUse ("None", "DisableUntilEnabled", "Cooldown" or "OneShot") and CooldownSeconds (needed above 0 for "Cooldown"). Pass a part's id to change only that part of an object the place gave several parts; leave it out for the object as a whole. An empty list leaves the object with nothing to do. Example: Interaction:SetOptions(door, {{Key = "Primary", Text = "Open door"}}) (server only) */
	setOptions(entity: Entity, options: ({ key?: string | undefined; text?: string | undefined; holdSeconds?: number | undefined; pressCount?: number | undefined; pressDrainPerSecond?: number | undefined; afterUse?: string | undefined; cooldownSeconds?: number | undefined })[], part?: string): void;
	/** Set how close a player must stand to the entity, in centimetres, for the prompt to show and the key to work. Clamped to 50..5000. A place's default is 250. (server only) */
	setRange(entity: Entity, range: number): void;
	/** Make one option of the entity unusable for this many seconds (up to 3600), for everyone, as if it had just been used with AfterUse "Cooldown". action is "Primary" or "Secondary". SetEnabled ends it early. Pass a part's id for an object with several parts; leave it out for the object as a whole. (server only) */
	startCooldown(entity: Entity, action: string, seconds: number, part?: string): void;
}
const Interaction: InteractionService;

/** The built-in leaderboard panel: players press Tab (or pick Leaderboard in the game menu) to see the top 10 of one of this place's ordered stores, with each player's name and picture (server only). */
interface LeaderboardService {
	/** Take the leaderboard panel down: players no longer see it until Configure shows a board again. The store keeps its entries. (server only) */
	clear(): void;
	/** Show the ordered store with this name (as DataStore:GetOrderedDataStore takes it) on the leaderboard panel: its top 10, highest first, or lowest first when options.ascending is true. Key each entry by the player's UserId and the panel shows their name and picture; any other key shows as it is. options.title (up to 32 characters, "Leaderboard" when left out) heads the panel and options.valueLabel (up to 16, "Score" when left out) heads the value column. The panel reads the store now and every 30 seconds while anyone is playing, so it trails the store by up to 30 seconds. Call it again to switch boards; one board shows at a time. (server only) */
	configure(store: string, options?: { title?: string | undefined; valueLabel?: string | undefined; ascending?: boolean | undefined } | undefined): void;
}
const Leaderboard: LeaderboardService;

/** Spawn lights at runtime (server only). Change any light through its Entity's Light members. */
interface LightsService {
	/** Spawn a light every player sees and return it as an Entity. kind is "Point", "Spot" or "Directional". props sets how it starts: Color ("#RRGGBB", white by default), Intensity (candelas, 8 by default; lux for a directional light, 10 by default), Range (cm, 1000 by default), InnerCone and OuterCone (degrees, 0 and 44 by default), Enabled, CastShadow (off by default: shadows are costly) and Rotation (a spot light points straight down and a directional light down at 45 degrees unless you give one). Numbers out of range are clamped. At most 64 spawned lights exist at once in a place; Destroy one to make room. (server only) */
	spawn(kind: string, position: Vector3, props?: { color?: string | undefined; intensity?: number | undefined; range?: number | undefined; innerCone?: number | undefined; outerCone?: number | undefined; enabled?: boolean | undefined; castShadow?: boolean | undefined; rotation?: Vector3 | undefined } | undefined): Entity;
}
const Lights: LightsService;

/** Spawn characters the server walks around the place and animates (server only). */
interface NPCsService {
	/** The NPC an Entity is (a World:Raycast hit, or NPC:GetEntity()), or nil when it is not a live NPC. (server only) */
	fromEntity(entity: Entity): NPC | undefined;
	/** Spawn a character standing at position (its feet), facing yaw degrees (0 faces +X), and return it as an NPC. Every player sees it. look.AvatarItemId optionally selects a published avatar body; omit it to spawn an NPC with no rendered body. SkinTone and EyeColor are avatar palette ids ("tone_05"), Height is a percent from 95 to 105. A place holds at most 50 NPCs at once; Destroy one to make room. (server only) */
	spawn(position: Vector3, look?: { avatarItemId?: string | undefined; skinTone?: string | undefined; eyeColor?: string | undefined; height?: number | undefined } | undefined, yaw?: number | undefined): NPC;
}
const NPCs: NPCsService;

/** Send messages between the server and players. */
interface NetService {
	/** Get the remote with this name (the same name on server and client refers to the same remote). */
	remote(name: string): Remote;
}
const Net: NetService;

/** Everyone in the session. */
interface PlayersService {
	/** The player on this machine. Always set in client scripts (they start once it exists); nil on a dedicated server. */
	readonly localPlayer: Player;
	/** A player joined after the handler connected. Does not replay existing players; use Players:OnPlayer to handle players already present and future joins. */
	readonly playerAdded: Signal<[player: Player]>;
	/** A player is leaving. */
	readonly playerRemoving: Signal<[player: Player]>;
	/** The player whose character this entity is right now, or nil. */
	getPlayerFromCharacter(character: Entity): Player | undefined;
	/** Players currently in the session. */
	getPlayers(): (Player)[];
	/** Call handler for every player: those already here and everyone who joins later. The usual way to set players up. */
	onPlayer(handler: (player: Player) => void): Connection;
}
const Players: PlayersService;

/** Facts about the running script. */
interface ScriptService {
	/** True when this script runs on a client. */
	readonly isClient: boolean;
	/** True when this script runs on the server. */
	readonly isServer: boolean;
	/** Display name: the script file's name without extension. */
	readonly name: string;
	/** The name this place's storage gives the script package this script belongs to, or "" for one of the place's own scripts. Example: Script.Package is "KillBricks" for Packages/KillBricks/Server/Main.luau. */
	readonly package: string;
	/** Path relative to the Scripts folder, e.g. "Server/Rounds.luau". */
	readonly path: string;
	/** The Script Points placed for this script's package: each an entity at the point's position, tagged with the package's storage name, its parameters readable with GetAttribute (numbers and true/false already converted, as the package declares them). Points that are switched off are left out. Empty for one of the place's own scripts. A package's scripts run once, not once per point: loop over Script.Points. Example: for _, point in Script.Points do print(point:GetAttribute("speed")) end */
	readonly points: (Entity)[];
	/** The settings this place gives this script's package, set once for the whole package in Place Settings > Storage: setting name to value, already converted as the package declares them (a number, true/false, or text; a choice is its text). A setting the place leaves unset has the package's default; one with no default and no value is missing (nil). Empty for one of the place's own scripts. A fresh table on every read. Example: local roundLength = Script.Settings.RoundLength */
	readonly settings: Record<string, boolean | number | string>;
	/** "server" or "client". */
	readonly side: string;
}
const Script: ScriptService;

/** Facts about the server this session runs on (server only). */
interface ServerService {
	/** The id of the private server this session belongs to, or nil for a public session. (server only) */
	readonly privateServerId: string | undefined;
	/** The Mantle account id (a plain UUID) of the private server's owner, or nil for a public session. (server only) */
	readonly privateServerOwnerId: string | undefined;
}
const Server: ServerService;

/** Play the sounds uploaded with this place: effects and loops, flat or placed in the world, and one music track per player. */
interface SoundsService {
	/** Play one of the sounds uploaded with this place (a sound emitter's Entity.SoundId, or its id in the place's assets). From a client script only this player hears it; from a server script every player does, or only params.Player. Give Position or AttachTo to place it in the world, where it fades to silence at Radius cm (4000 by default); without either it plays flat, like interface sounds. Volume 0 to 2 (1), Pitch 0.5 to 2 (1), Looped (false), FadeIn seconds (0). A sound that has not finished downloading starts when it arrives. Returns a SoundHandle. */
	play(sound: string, params?: { volume?: number | undefined; pitch?: number | undefined; looped?: boolean | undefined; position?: Vector3 | undefined; attachTo?: Entity | undefined; radius?: number | undefined; fadeIn?: number | undefined; player?: Player | undefined } | undefined): SoundHandle;
	/** Play one of the place's sounds as music: from a client script this player's, from a server script every player's, or only params.Player's. Each player has one music track: a new one crossfades from the old over FadeIn seconds (1 by default), and playing the track already playing only changes its Volume (0 to 2). Loops unless Looped is false. */
	playMusic(sound: string, params?: { volume?: number | undefined; looped?: boolean | undefined; fadeIn?: number | undefined; player?: Player | undefined } | undefined): void;
	/** Stop the music, fading it out over params.FadeOut seconds (0, the default, stops it at once). From a server script: every player's music, or only params.Player's. */
	stopMusic(params?: { fadeOut?: number | undefined; player?: Player | undefined } | undefined): void;
}
const Sounds: SoundsService;

/** Shared game values the server sets and everyone can read (score, round, phase). */
interface StateService {
	/** A value changed (fires on the server and on every client). */
	readonly changed: Signal<[key: string, value: any]>;
	/** The current value, or nil. */
	get(key: string): any;
	/** Call handler with the value now and every time it changes. The easy way to keep UI in sync. */
	observe(key: string, handler: (value: any) => void): Connection;
	/** Set a value for everyone. Numbers, strings, booleans, Vector3 and tables work. (server only) */
	set(key: string, value: any): void;
}
const State: StateService;

/** The library items (vehicles, weapons and script packages) this place stores, by the names its storage gives them. */
interface StorageService {
	/** The library item id (itm_u...) of the entry this place's storage declares under name. The name is case-sensitive. Pass the id to Vehicles:Spawn (a stored vehicle) or Backpack:Grant (a stored weapon): they accept only an id the place stores. Errors when the place's storage has no entry with that name. */
	get(name: string): string;
}
const Storage: StorageService;

/** Clocks for scripts. */
interface TimeService {
	/** A high-precision timer in seconds, for measuring how long something takes. */
	readonly clock: number;
	/** Seconds since the game started, synced to the server's clock on every machine. */
	readonly now: number;
}
const Time: TimeService;

/** The local player's on-screen UI: a simple HUD line/toast, and the retained node tree behind real widgets. */
interface UIService {
	/** Whether the player wants captions shown. */
	readonly captions: boolean;
	/** Whether the player asked for a high-contrast presentation. */
	readonly highContrast: boolean;
	/** The player's current locale (e.g. "fr-CA"). */
	readonly locale: string;
	/** Whether the player asked for reduced motion. */
	readonly reducedMotion: boolean;
	/** The player's platform text-scale preference (0.85-1.5). */
	readonly textScale: number;
	/** The player's remapped label for an action name (e.g. the bound key), or the action name itself if unset. (client only) */
	actionLabel(action: string): string;
	/** Create a new node: "frame", "text", "image", "button", "toggle", "list", "grid", "scroll" or "prompt". Nil means the per-script node budget (256) is exhausted. (client only) */
	create(kind: string): UiNode | undefined;
	/** Format a number for display using the player's current locale. (client only) */
	formatNumber(value: number): string;
	/** Open one of the place's UI documents (a .rml file in its UI/ folder) on this player's screen. Mode is "modal" (the default: gameplay input stops and Back closes it), "overlay" (play goes on; the mouse works on its controls) or "hud" (drawn only, never takes input). An unknown path or mode is an error, and so is opening on a server. Example: local shop = UI:OpenDocument("UI/shop.rml") (client only) */
	openDocument(path: string, mode?: string | undefined): UiDocument;
	/** Force the mouse cursor on or off regardless of whether an interactive node is visible right now. Nil goes back to automatic: shown only while a Button/Toggle/Prompt is visible, enabled and has an action. A gamepad-only panel can call SetCursorVisible(false) to keep working through focus/Accept without ever drawing a cursor. (client only) */
	setCursorVisible(visible?: boolean | undefined): void;
	/** Show text in a named HUD line (same slot replaces the previous text; empty text clears it). (client only) */
	setText(slot: string, text: string): void;
	/** Pop up a short message. (client only) */
	toast(text: string, seconds?: number | undefined): void;
}
const UI: UIService;

/** Spawn and seat cars: the vehicles in the place's storage (server only). */
interface VehiclesService {
	/** Put the player back on foot beside the car they are driving. Returns false when they are not driving, the car is still moving, or there is nowhere beside it to stand. (server only) */
	eject(player: Player): boolean;
	/** The car's speed in centimetres per second, as the simulation reports it. Errors if the entity is not a car or no longer exists. (server only) */
	getSpeed(vehicle: Entity): number;
	/** Whether the car is locked. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Errors if the entity is not a car or no longer exists. (server only) */
	isLocked(vehicle: Entity): boolean;
	/** Lock the car. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car. (server only) */
	lock(vehicle: Entity): boolean;
	/** Put the player in the driver's seat, through the same checks as walking up and pressing the prompt: the player must be on foot, alive, near the car with a clear line to the seat, and the seat must be empty. Returns false when any of that refuses. (server only) */
	seat(vehicle: Entity, player: Player): boolean;
	/** Put one of the car's liveries on it: index counts from 0 in the liveries its vehicle was published with, and -1 takes the livery off. Every player sees the change. Returns false when the entity is not a car or the car has no livery at that index (the built-in cars have none). (server only) */
	setLivery(vehicle: Entity, index: number): boolean;
	/** Paint the car: color is "#RRGGBB", the same format as Backpack:SetWeaponColor. Returns false when the entity is not a car. (server only) */
	setPaintColor(vehicle: Entity, color: string): boolean;
	/** Spawn a car at position and return it as an Entity. item is the library item id of a vehicle this place stores: the string Storage:Get returns for one of the place's storage names. Any other value errors -- an asset id, an object path, a built-in car's name, an item the place does not store, or a weapon. The car is a real replicated vehicle: every player sees it, walks up to it and can drive it. Read its Position, tag it, or Destroy it like any entity. (server only) */
	spawn(item: string, position: Vector3): Entity;
	/** Unlock the car, so anyone can open it again. A locked car opens only for the last player who drove it: anyone else's vehicle key rattles the door, and holding the key breaks in -- the window on that side breaks, the car unlocks, and it must be hotwired before it starts. The lock stays when the driver gets out, and its driver can also lock and unlock it from the seat (L / D-pad left). Returns false when the entity is not a car. (server only) */
	unlock(vehicle: Entity): boolean;
}
const Vehicles: VehiclesService;

/** Find and watch the things in the level. */
interface WorldService {
	/** Sweep a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Takes the same params as World:Raycast. */
	blockcast(origin: Vector3, size: Vector3, direction: Vector3, params?: { maxDistance?: number | undefined; include?: (Entity | Player)[] | undefined; exclude?: (Entity | Player)[] | undefined; tag?: string | undefined; ignorePlayers?: boolean | undefined }): { entity: Entity; player?: Player | undefined; position: Vector3; normal: Vector3; distance: number } | undefined;
	/** Every entity with this actor tag. */
	findByTag(tag: string): (Entity)[];
	/** The first entity whose name (actor label) matches, or nil. */
	findFirst(name: string): Entity | undefined;
	/** Every entity and player character touching a box of this size (its full width, depth and height, at most 100 m on any side, kept square to the world's axes) centred on center, nearest first. Takes the same params as World:GetEntitiesInRadius. */
	getEntitiesInBox(center: Vector3, size: Vector3, params?: { include?: (Entity | Player)[] | undefined; exclude?: (Entity | Player)[] | undefined; tag?: string | undefined; ignorePlayers?: boolean | undefined; maxResults?: number | undefined }): (Entity)[];
	/** Every entity and player character touching a ball of this radius (at most 50 m) around center, nearest first. Overlap-only entities (triggers) count; entities with no collision never do. A character is listed as its Entity; Players:GetPlayerFromCharacter tells whose it is. params: Include, Exclude, Tag, IgnorePlayers, and MaxResults (256 at most, and by default). Runs on this machine only; a script may make 256 spatial queries a frame. */
	getEntitiesInRadius(center: Vector3, radius: number, params?: { include?: (Entity | Player)[] | undefined; exclude?: (Entity | Player)[] | undefined; tag?: string | undefined; ignorePlayers?: boolean | undefined; maxResults?: number | undefined }): (Entity)[];
	/** Call handler for every entity that has the tag now, and for each one that gets it later. */
	onTagged(tag: string, handler: (entity: Entity) => void): Connection;
	/** Call handler when an entity loses the tag. */
	onUntagged(tag: string, handler: (entity: Entity) => void): Connection;
	/** Cast a ray from origin along direction and return the first thing it hits: a table of Entity, Player, Position, Normal and Distance, or nil. Player is set when the hit is a player's character. The ray is as long as direction, or params.MaxDistance when given, and at most 1 km. It passes through overlap-only entities (triggers) and never hits entities with no collision. params: MaxDistance, Include (only these entities or players count), Exclude, Tag (only entities with this tag count), IgnorePlayers. Runs on this machine only; a script may make 256 spatial queries a frame. */
	raycast(origin: Vector3, direction: Vector3, params?: { maxDistance?: number | undefined; include?: (Entity | Player)[] | undefined; exclude?: (Entity | Player)[] | undefined; tag?: string | undefined; ignorePlayers?: boolean | undefined }): { entity: Entity; player?: Player | undefined; position: Vector3; normal: Vector3; distance: number } | undefined;
	/** Sweep a ball of this radius (at most 50 m) from origin along direction and return the first thing it touches, in the same table as World:Raycast, or nil. Distance is how far the ball's centre travelled. Takes the same params as World:Raycast. */
	spherecast(origin: Vector3, radius: number, direction: Vector3, params?: { maxDistance?: number | undefined; include?: (Entity | Player)[] | undefined; exclude?: (Entity | Player)[] | undefined; tag?: string | undefined; ignorePlayers?: boolean | undefined }): { entity: Entity; player?: Player | undefined; position: Vector3; normal: Vector3; distance: number } | undefined;
}
const World: WorldService;

const task: {
	wait(seconds?: number): Promise<number>;
	spawn(fn: (...args: any[]) => any, ...args: any[]): void;
	delay(seconds: number, fn: (...args: any[]) => any, ...args: any[]): number;
	defer(fn: (...args: any[]) => any, ...args: any[]): number;
	cancel(id: number): void;
};

function setTimeout(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number;
function setInterval(fn: (...args: any[]) => void, ms?: number, ...args: any[]): number;
function clearTimeout(id: number): void;
function clearInterval(id: number): void;

const console: {
	log(...args: any[]): void;
	info(...args: any[]): void;
	warn(...args: any[]): void;
	error(...args: any[]): void;
};

} // declare global
